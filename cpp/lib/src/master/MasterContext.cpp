/*
 * Copyright 2013-2022 Step Function I/O, LLC
 *
 * Licensed to Green Energy Corp (www.greenenergycorp.com) and Step Function I/O
 * LLC (https://stepfunc.io) under one or more contributor license agreements.
 * See the NOTICE file distributed with this work for additional information
 * regarding copyright ownership. Green Energy Corp and Step Function I/O LLC license
 * this file to you under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may obtain
 * a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "MasterContext.h"

#include "DeleteFileTask.h"
#include "GetFileInfoTask.h"
#include "GetFilesInDirectoryTask.h"
#include "ReadFileTask.h"
#include "WriteFileTask.h"
#include "app/APDUBuilders.h"
#include "app/APDULogging.h"
#include "app/parsing/APDUHeaderParser.h"
#include "gen/objects/Group12.h"
#include "link/LinkHeader.h"
#include "logging/LogMacros.h"
#include "master/CommandTask.h"
#include "master/EmptyResponseTask.h"
#include "master/MeasurementHandler.h"
#include "master/RestartOperationTask.h"
#include "master/UserPollTask.h"

#include "opendnp3/logging/LogLevels.h"
#include "transport/TransportHeader.h"

#include <utility>

namespace opendnp3
{
MContext::MContext(const Addresses& addresses,
                   const Logger& logger,
                   std::shared_ptr<exe4cpp::IExecutor> executor,
                   std::shared_ptr<ILowerLayer> lower,
                   const std::shared_ptr<ISOEHandler>& SOEHandler,
                   const std::shared_ptr<IMasterApplication>& application,
                   std::shared_ptr<IMasterScheduler> scheduler,
                   const MasterParams& params,
                   std::shared_ptr<IOHandlersManager> iohandlersManager)
    : logger(logger),
      executor(std::move(executor)),
      lower(std::move(lower)),
      addresses(addresses),
      params(params),
      SOEHandler(SOEHandler),
      application(application),
      scheduler(std::move(scheduler)),
      tasks(params, logger, *application, SOEHandler),
      txBuffer(params.maxTxFragSize),
      tstate(TaskState::IDLE),
      iohandlersManager(std::move(iohandlersManager))
{
    FileTransferMaxRxBlockSize = params.maxRxFragSize
                               - LinkHeader::HEADER_SIZE
                               - TransportHeader::HEADER_SIZE
                               - APDUHeader::RESPONSE_SIZE
                               - 3;

    FileTransferMaxTxBlockSize = params.maxTxFragSize
                               - LinkHeader::HEADER_SIZE
                               - TransportHeader::HEADER_SIZE
                               - APDUHeader::REQUEST_SIZE
                               - 3;
    this->iohandlersManager->ChannelReservationChanged.connect([app = this->application](const bool isBackup) {
        if (app)
        {
            app->OnChannelReservationChanged(isBackup);
        }
    });
}

std::shared_ptr<MContext> MContext::Create(
    const Addresses& addresses,
    const Logger& logger,
    const std::shared_ptr<exe4cpp::IExecutor>& executor,
    std::shared_ptr<ILowerLayer> lower,
    const std::shared_ptr<ISOEHandler>& SOEHandler,
    const std::shared_ptr<IMasterApplication>& application,
    std::shared_ptr<IMasterScheduler> scheduler,
    const MasterParams& params,
    const std::shared_ptr<IOHandlersManager>& iohandlersManager)
{
    auto ptr = std::shared_ptr<MContext>(new MContext(addresses, logger, executor, std::move(lower), SOEHandler, application, std::move(scheduler), params, iohandlersManager));
    std::weak_ptr<MContext> weakPtr = ptr;
    ptr->iohandlersManager->SetChannelStateChangedCallback([weakPtr](const bool channelDown) {
        const auto shared = weakPtr.lock();
        if (!shared)
        {
            return;
        }
        shared->application->OnStateChange(channelDown ? LinkStatus::UNRESET : LinkStatus::RESET, LinkStateChangeSource::Unconditional);
    });
    return ptr;
}

bool MContext::OnLowerLayerUp()
{
    std::lock_guard<std::mutex> lock{ _mtx };
    if (isOnline)
    {
        return false;
    }

    isOnline = true;

    tasks.Initialize(*this->scheduler, *this);

    this->application->OnOpen();

    return true;
}

bool MContext::OnLowerLayerDown()
{
    std::lock_guard<std::mutex> lock{ _mtx };
    if (!isOnline)
    {
        return false;
    }

    tstate = TaskState::IDLE;
    responseTimer.cancel();
    solSeq = unsolSeq = 0;
    isOnline = isSending = false;
    activeTask.reset();

    this->scheduler->SetRunnerOffline(*this);
    this->application->OnClose();

    return true;
}

bool MContext::OnReceive(const Message& message)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    if (!this->isOnline)
    {
        SIMPLE_LOG_BLOCK(this->logger, flags::ERR, "Ignorning rx data while offline");
        return false;
    }

    if (message.addresses.destination != this->addresses.source)
    {
        FORMAT_LOG_BLOCK(this->logger, flags::WARN, "Unknown destination address: %u", message.addresses.destination);
        return false;
    }

    if (message.addresses.source != this->addresses.destination)
    {
        FORMAT_LOG_BLOCK(this->logger, flags::WARN, "Unexpected message source: %u", message.addresses.source);
        return false;
    }

    const auto result = APDUHeaderParser::ParseResponse(message.payload, &this->logger);
    if (!result.success)
    {
        return true;
    }

    logging::LogHeader(this->logger, flags::APP_HEADER_RX, result.header);

    this->OnParsedHeader(message.payload, result.header, result.objects);

    return true;
}

bool MContext::OnTxReady()
{
    std::lock_guard<std::mutex> lock{ _mtx };
    if (!this->isOnline || !this->isSending)
    {
        return false;
    }

    this->isSending = false;

    this->tstate = this->OnTransmitComplete();
    this->CheckConfirmTransmit();

    return true;
}

void MContext::OnResponseTimeout()
{
    if (isOnline)
    {
        this->tstate = this->OnResponseTimeoutEvent();
    }
}

void MContext::CompleteActiveTask()
{
    this->activeTask.reset();
    this->scheduler->CompleteCurrentFor(*this);
}

void MContext::OnParsedHeader(const ser4cpp::rseq_t& /*apdu*/,
                              const APDUResponseHeader& header,
                              const ser4cpp::rseq_t& objects)
{
    // Note: this looks silly, but OnParsedHeader() is virtual and can be overriden to do SA
    this->ProcessAPDU(header, objects);
}

/// --- command handlers ----

void MContext::DirectOperate(CommandSet&& commands, const CommandResultCallbackT& callback, const TaskConfig& config)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    const auto timeout = Timestamp(this->executor->get_time()) + params.taskStartTimeout;
    this->ScheduleAdhocTask(CommandTask::CreateDirectOperate(this->tasks.context, std::move(commands),
                                                             this->params.controlQualifierMode, *application, callback,
                                                             timeout, config, logger));
}

void MContext::SelectAndOperate(CommandSet&& commands, const CommandResultCallbackT& callback, const TaskConfig& config)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    const auto timeout = Timestamp(this->executor->get_time()) + params.taskStartTimeout;
    this->ScheduleAdhocTask(CommandTask::CreateSelectAndOperate(this->tasks.context, std::move(commands),
                                                                this->params.controlQualifierMode, *application,
                                                                callback, timeout, config, logger));
}

void MContext::Select(CommandSet&& commands, const CommandResultCallbackT& callback, const TaskConfig& config)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    const auto timeout = Timestamp(this->executor->get_time()) + params.taskStartTimeout;
    this->ScheduleAdhocTask(CommandTask::CreateSelect(this->tasks.context, std::move(commands),
                                                      this->params.controlQualifierMode, *application, callback,
                                                      timeout, config, logger));
}

void MContext::Operate(CommandSet&& commands, const CommandResultCallbackT& callback, const TaskConfig& config)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    const auto timeout = Timestamp(this->executor->get_time()) + params.taskStartTimeout;
    this->ScheduleAdhocTask(CommandTask::CreateOperate(this->tasks.context, std::move(commands),
                                                      this->params.controlQualifierMode, *application, callback,
                                                      timeout, config, logger));
}

void MContext::ProcessAPDU(const APDUResponseHeader& header, const ser4cpp::rseq_t& objects)
{
    switch (header.function)
    {
    case (FunctionCode::UNSOLICITED_RESPONSE):
        this->ProcessUnsolicitedResponse(header, objects);
        break;
    case (FunctionCode::RESPONSE):
        this->ProcessResponse(header, objects);
        break;
    default:
        FORMAT_LOG_BLOCK(this->logger, flags::WARN, "Ignoring unsupported function code: %s",
                         FunctionCodeSpec::to_human_string(header.function));
        break;
    }
}

void MContext::ProcessIIN(const IINField& iin)
{
    if (iin.IsSet(IINBit::DEVICE_RESTART) && !this->params.ignoreRestartIIN)
    {
        this->tasks.OnRestartDetected();
        this->scheduler->Evaluate();
    }

    if (iin.IsSet(IINBit::EVENT_BUFFER_OVERFLOW) && this->params.integrityOnEventOverflowIIN)
    {
        if (this->tasks.DemandIntegrity())
            this->scheduler->Evaluate();
    }

    if (iin.IsSet(IINBit::NEED_TIME))
    {
        if (this->tasks.DemandTimeSync())
            this->scheduler->Evaluate();
    }

    if ((iin.IsSet(IINBit::CLASS1_EVENTS) && this->params.eventScanOnEventsAvailableClassMask.HasClass1())
        || (iin.IsSet(IINBit::CLASS2_EVENTS) && this->params.eventScanOnEventsAvailableClassMask.HasClass2())
        || (iin.IsSet(IINBit::CLASS3_EVENTS) && this->params.eventScanOnEventsAvailableClassMask.HasClass3()))
    {
        if (this->tasks.DemandEventScan())
            this->scheduler->Evaluate();
    }

    this->application->OnReceiveIIN(iin);
}

void MContext::ProcessUnsolicitedResponse(const APDUResponseHeader& header, const ser4cpp::rseq_t& objects)
{
    if (!header.control.UNS)
    {
        SIMPLE_LOG_BLOCK(logger, flags::WARN, "Ignoring unsolicited response without UNS bit set");
        return;
    }

    auto result = MeasurementHandler::ProcessMeasurements(header.as_response_info(), objects, logger, SOEHandler.get());

    if ((result == ParseResult::OK) && header.control.CON)
    {
        this->QueueConfirm(APDUHeader::UnsolicitedConfirm(header.control.SEQ));
    }

    this->ProcessIIN(header.IIN);
}

void MContext::ProcessResponse(const APDUResponseHeader& header, const ser4cpp::rseq_t& objects)
{
    this->tstate = this->OnResponseEvent(header, objects);
    this->ProcessIIN(header.IIN); // TODO - should we process IIN bits for unexpected responses?
}

void MContext::QueueConfirm(const APDUHeader& header)
{
    this->confirmQueue.push_back(header);
    this->CheckConfirmTransmit();
}

bool MContext::CheckConfirmTransmit()
{
    if (this->isSending || this->confirmQueue.empty() || !iohandlersManager->PrepareChannel(true))
    {
        return false;
    }

    const auto confirm = this->confirmQueue.front();
    APDUWrapper wrapper(this->txBuffer.as_wslice());
    wrapper.SetFunction(confirm.function);
    wrapper.SetControl(confirm.control);
    if (statisticsChangeHandler) {
        statisticsChangeHandler(iohandlersManager->IsBackupChannelUsed(), StatisticsValueType::ConfirmationsSent, 1);
    }
    this->Transmit(wrapper.ToRSeq());
    this->confirmQueue.pop_front();
    return true;
}

void MContext::Transmit(const ser4cpp::rseq_t& data)
{
    if (iohandlersManager->PrepareChannel(true)) {
        logging::ParseAndLogRequestTx(this->logger, data);
        assert(!this->isSending);
        this->isSending = this->lower->BeginTransmit(Message(this->addresses, data));
    }
}

bool MContext::DemandTimeSyncronization()
{
    std::lock_guard<std::mutex> lock{ _mtx };
    const bool result = this->tasks.DemandTimeSync();
    if (result) {
        this->scheduler->Evaluate();
    }
    return result;
}

bool MContext::ReadFile(const std::string& sourceFile, FileOperationTaskCallbackT callback)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    const auto task = std::make_shared<ReadFileTask>(this->tasks.context, *this->application, this->logger, sourceFile,
                                                     callback, FileTransferMaxRxBlockSize);
    this->ScheduleAdhocTask(task);
    return true;
}

bool MContext::WriteFile(std::shared_ptr<std::ifstream> source, const std::string& destFilename, FileOperationTaskCallbackT callback)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    const auto task = std::make_shared<WriteFileTask>(this->tasks.context, *this->application, this->logger,
                                                      source, destFilename, FileTransferMaxTxBlockSize, callback);
    this->ScheduleAdhocTask(task);
    return true;
}

void MContext::GetFilesInDirectory(const std::string& sourceDirectory, const GetFilesInfoTaskCallbackT& callback)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    const auto task = std::make_shared<GetFilesInDirectoryTask>(this->tasks.context, *this->application, this->logger, sourceDirectory,
                                                                callback, FileTransferMaxRxBlockSize);
    return this->ScheduleAdhocTask(task);
}

void MContext::GetFileInfo(const std::string& sourceFile, const GetFilesInfoTaskCallbackT& callback)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    const auto task = std::make_shared<GetFileInfoTask>(this->tasks.context, *this->application, this->logger, sourceFile, callback);
    return this->ScheduleAdhocTask(task);
}

void MContext::DeleteFileFunction(const std::string& filename, FileOperationTaskCallbackT callback)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    const auto task = std::make_shared<DeleteFileTask>(this->tasks.context, *this->application, this->logger, filename, callback);
    return this->ScheduleAdhocTask(task);
}

void MContext::AddStatisticsHandler(const StatisticsChangeHandler_t& changeHandler)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    statisticsChangeHandler = changeHandler;
}

void MContext::RemoveStatisticsHandler()
{
    std::lock_guard<std::mutex> lock{ _mtx };
    statisticsChangeHandler = nullptr;
}

void MContext::StartResponseTimer()
{
    auto timeout = [self = shared_from_this()]() { self->OnResponseTimeout(); };
    this->responseTimer = this->executor->start(this->params.responseTimeout.value, timeout);
}

std::shared_ptr<IMasterTask> MContext::AddScan(TimeDuration period,
                                               const HeaderBuilderT& builder,
                                               std::shared_ptr<ISOEHandler> soe_handler,
                                               TaskConfig config)
{
    auto task = std::make_shared<UserPollTask>(
        this->tasks.context, builder,
        TaskBehavior::ImmediatePeriodic(period, params.taskRetryPeriod, params.maxTaskRetryPeriod, params.retryCount), true, *application,
        soe_handler, logger, config
    );
    this->ScheduleRecurringPollTask(task);
    return task;
}

std::shared_ptr<IMasterTask> MContext::AddClassScan(const ClassField& field,
                                                    TimeDuration period,
                                                    std::shared_ptr<ISOEHandler> soe_handler,
                                                    TaskConfig config)
{
    auto build = [field](HeaderWriter& writer) -> bool { return build::WriteClassHeaders(writer, field); };
    return this->AddScan(period, build, soe_handler, config);
}

std::shared_ptr<IMasterTask> MContext::AddAllObjectsScan(GroupVariationID gvId,
                                                         TimeDuration period,
                                                         std::shared_ptr<ISOEHandler> soe_handler,
                                                         TaskConfig config)
{
    auto build = [gvId](HeaderWriter& writer) -> bool { return writer.WriteHeader(gvId, QualifierCode::ALL_OBJECTS); };
    return this->AddScan(period, build, soe_handler, config);
}

std::shared_ptr<IMasterTask> MContext::AddRangeScan(GroupVariationID gvId,
                                                    uint16_t start,
                                                    uint16_t stop,
                                                    TimeDuration period,
                                                    std::shared_ptr<ISOEHandler> soe_handler,
                                                    TaskConfig config)
{
    auto build = [gvId, start, stop](HeaderWriter& writer) -> bool {
        return writer.WriteRangeHeader<ser4cpp::UInt16>(QualifierCode::UINT16_START_STOP, gvId, start, stop);
    };
    return this->AddScan(period, build, soe_handler, config);
}

void MContext::Scan(const HeaderBuilderT& builder, std::shared_ptr<ISOEHandler> soe_handler, TaskConfig config)
{
    const auto timeout = Timestamp(this->executor->get_time()) + params.taskStartTimeout;

    auto task
        = std::make_shared<UserPollTask>(this->tasks.context, builder, TaskBehavior::SingleExecutionNoRetry(timeout),
                                         false, *application, soe_handler, logger, config);

    this->ScheduleAdhocTask(task);
}

void MContext::ScanClasses(const ClassField& field, std::shared_ptr<ISOEHandler> soe_handler, TaskConfig config)
{
    auto configure = [field](HeaderWriter& writer) -> bool { return build::WriteClassHeaders(writer, field); };
    this->Scan(configure, soe_handler, config);
}

void MContext::ScanAllObjects(GroupVariationID gvId, std::shared_ptr<ISOEHandler> soe_handler, TaskConfig config)
{
    auto configure
        = [gvId](HeaderWriter& writer) -> bool { return writer.WriteHeader(gvId, QualifierCode::ALL_OBJECTS); };
    this->Scan(configure, soe_handler, config);
}

void MContext::ScanRange(
    GroupVariationID gvId, uint16_t start, uint16_t stop, std::shared_ptr<ISOEHandler> soe_handler, TaskConfig config)
{
    auto configure = [gvId, start, stop](HeaderWriter& writer) -> bool {
        return writer.WriteRangeHeader<ser4cpp::UInt16>(QualifierCode::UINT16_START_STOP, gvId, start, stop);
    };
    this->Scan(configure, soe_handler, config);
}

void MContext::Write(const TimeAndInterval& value, uint16_t index, TaskConfig config)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    auto builder = [value, index](HeaderWriter& writer) -> bool {
        return writer.WriteSingleIndexedValue<ser4cpp::UInt16, TimeAndInterval>(QualifierCode::UINT16_CNT_UINT16_INDEX,
                                                                                Group50Var4::Inst(), value, index);
    };

    const auto timeout = Timestamp(this->executor->get_time()) + params.taskStartTimeout;
    auto task = std::make_shared<EmptyResponseTask>(this->tasks.context, *this->application, "WRITE TimeAndInterval",
                                                    FunctionCode::WRITE, builder, timeout, this->logger, config);
    this->ScheduleAdhocTask(task);
}

void MContext::Restart(RestartType op, const RestartOperationCallbackT& callback, TaskConfig config)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    const auto timeout = Timestamp(this->executor->get_time()) + params.taskStartTimeout;
    auto task = std::make_shared<RestartOperationTask>(this->tasks.context, *this->application, timeout, op, callback,
                                                       this->logger, config);
    this->ScheduleAdhocTask(task);
}

void MContext::PerformFunction(const std::string& name,
                               FunctionCode func,
                               const HeaderBuilderT& builder,
                               TaskConfig config)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    const auto timeout = Timestamp(this->executor->get_time()) + params.taskStartTimeout;
    auto task = std::make_shared<EmptyResponseTask>(this->tasks.context, *this->application, name, func, builder,
                                                    timeout, this->logger, config);
    this->ScheduleAdhocTask(task);
}

bool MContext::Run(const std::shared_ptr<IMasterTask>& task)
{
    if (this->activeTask || this->tstate != TaskState::IDLE)
    {
        return false;
    }
    this->tstate = TaskState::TASK_READY;
    this->activeTask = task;
    if (this->iohandlersManager)
    {
        if (!this->iohandlersManager->PrepareChannel(task->CanBeExecutedOnBackupChannel()))
        {
            FORMAT_LOG_BLOCK(
                logger,
                flags::INFO,
                "Cannot run task on %s connection. Task name = \"%s\"",
                this->iohandlersManager->IsBackupChannelUsed() ? "backup" : "primary",
                this->activeTask ? this->activeTask->Name() : ""
            )
            if (this->iohandlersManager->IsBackupChannelUsed() && !task->CanBeExecutedOnBackupChannel())
            {
                const auto now = Timestamp(this->executor->get_time());
                this->activeTask->DelayByPeriod(now);
            }
            this->CompleteActiveTask();
            this->tstate = TaskState::IDLE;
            return false;
        }
    }
    const bool isTaskStarted = this->activeTask->OnStart(Timestamp(executor->get_time()));
    if (!isTaskStarted) {
        this->CompleteActiveTask();
        this->tstate = TaskState::IDLE;
    }
    else {
        FORMAT_LOG_BLOCK(logger, flags::INFO, "Begining task: %s", this->activeTask->Name())

        if (!this->isSending)
        {
            this->tstate = this->ResumeActiveTask();
        }
    }
    return true;
}

/// ------ private helpers ----------

void MContext::ScheduleRecurringPollTask(const std::shared_ptr<IMasterTask>& task)
{
    this->tasks.BindTask(task);

    if (this->isOnline)
    {
        this->scheduler->Add(task, *this);
    }
}

void MContext::ScheduleAdhocTask(const std::shared_ptr<IMasterTask>& task)
{
    if (this->isOnline)
    {
        this->scheduler->Add(task, *this);
    }
    else
    {
        // can't run this task since we're offline so fail it immediately
        task->OnLowerLayerClose(Timestamp(this->executor->get_time()));
        if (this->iohandlersManager) {
            this->iohandlersManager->NotifyTaskResult(
                false,
                task->GetTaskType() == MasterTaskType::USER_POLL
            );
        }
    }
}

MContext::TaskState MContext::ResumeActiveTask()
{
    APDURequest request(this->txBuffer.as_wslice());

    /// try to build a requst for the task
    if (!this->activeTask->BuildRequest(request, this->solSeq))
    {
        activeTask->OnMessageFormatError(Timestamp(executor->get_time()));
        this->CompleteActiveTask();
        return TaskState::IDLE;
    }

    this->StartResponseTimer();
    auto apdu = request.ToRSeq();
    this->RecordLastRequest(apdu);
    this->Transmit(apdu);

    return TaskState::WAIT_FOR_RESPONSE;
}

//// --- State tables ---

MContext::TaskState MContext::OnTransmitComplete()
{
    switch (tstate)
    {
    case (TaskState::TASK_READY):
        return this->ResumeActiveTask();
    default:
        return tstate;
    }
}

MContext::TaskState MContext::OnResponseEvent(const APDUResponseHeader& header, const ser4cpp::rseq_t& objects)
{
    switch (tstate)
    {
    case (TaskState::WAIT_FOR_RESPONSE):
        return OnResponse_WaitForResponse(header, objects);
    default:
        FORMAT_LOG_BLOCK(logger, flags::WARN, "Not expecting a response, sequence: %u", header.control.SEQ);
        return tstate;
    }
}

MContext::TaskState MContext::OnResponseTimeoutEvent()
{
    switch (tstate)
    {
    case (TaskState::WAIT_FOR_RESPONSE):
        return OnResponseTimeout_WaitForResponse();
    default:
        SIMPLE_LOG_BLOCK(logger, flags::ERR, "Unexpected response timeout");
        return tstate;
    }
}

//// --- State actions ----

MContext::TaskState MContext::StartTask_TaskReady()
{
    if (this->isSending)
    {
        return TaskState::TASK_READY;
    }

    return this->ResumeActiveTask();
}

MContext::TaskState MContext::OnResponse_WaitForResponse(const APDUResponseHeader& header,
                                                         const ser4cpp::rseq_t& objects)
{
    if (header.control.SEQ != this->solSeq)
    {
        FORMAT_LOG_BLOCK(this->logger, flags::WARN, "Response with bad sequence: %u", header.control.SEQ);
        return TaskState::WAIT_FOR_RESPONSE;
    }

    this->responseTimer.cancel();

    this->solSeq.Increment();

    auto now = Timestamp(this->executor->get_time());

    if (header.function == FunctionCode::CONFIRM && statisticsChangeHandler) {
        statisticsChangeHandler(iohandlersManager->IsBackupChannelUsed(), StatisticsValueType::ConfirmationsReceived, 1);
    }

    const auto result = this->activeTask->OnResponse(header, objects, now);

    if (header.control.CON)
    {
        this->QueueConfirm(APDUHeader::SolicitedConfirm(header.control.SEQ));
    }

    switch (result)
    {
    case (IMasterTask::ResponseResult::OK_CONTINUE):
        this->StartResponseTimer();
        return TaskState::WAIT_FOR_RESPONSE;
    case (IMasterTask::ResponseResult::OK_REPEAT):
        return StartTask_TaskReady();
    default:
        if (this->iohandlersManager) {
            this->iohandlersManager->NotifyTaskResult(
                true,
                this->activeTask->GetTaskType() == MasterTaskType::USER_POLL
            );
        }
        // task completed or failed, either way go back to idle
        this->CompleteActiveTask();
        return TaskState::IDLE;
    }
}

MContext::TaskState MContext::OnResponseTimeout_WaitForResponse()
{
    FORMAT_LOG_BLOCK(logger, flags::WARN, "Timeout waiting for response, task - %s", this->activeTask->Name());

    const auto now = Timestamp(this->executor->get_time());
    this->activeTask->OnResponseTimeout(now);
    this->solSeq.Increment();
    if (this->activeTask->OutOfRetries() && this->iohandlersManager)
    {
        this->iohandlersManager->NotifyTaskResult(false, this->activeTask ? this->activeTask->GetTaskType() == MasterTaskType::USER_POLL : false);
    }
    this->CompleteActiveTask();
    return TaskState::IDLE;
}
} // namespace opendnp3
