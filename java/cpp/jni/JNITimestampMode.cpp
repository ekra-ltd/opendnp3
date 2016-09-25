//
//  _   _         ______    _ _ _   _             _ _ _
// | \ | |       |  ____|  | (_) | (_)           | | | |
// |  \| | ___   | |__   __| |_| |_ _ _ __   __ _| | | |
// | . ` |/ _ \  |  __| / _` | | __| | '_ \ / _` | | | |
// | |\  | (_) | | |___| (_| | | |_| | | | | (_| |_|_|_|
// |_| \_|\___/  |______\__,_|_|\__|_|_| |_|\__, (_|_|_)
//                                           __/ |
//                                          |___/
// 
// This file is auto-generated. Do not edit manually
// 
// Copyright 2016 Automatak LLC
// 
// Automatak LLC (www.automatak.com) licenses this file
// to you under the the Apache License Version 2.0 (the "License"):
// 
// http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "JNITimestampMode.h"

namespace jni
{
    bool TimestampMode::init(JNIEnv* env)
    {

        this->clazz = env->FindClass("Lcom/automatak/dnp3/enums/TimestampMode;");
        if(!this->clazz) return false;

        this->fromTypeMethod = env->GetStaticMethodID(this->clazz, "fromType", "(I)Lcom/automatak/dnp3/enums/TimestampMode;");
        if(!this->fromTypeMethod) return false;

        return true;
    }

    jobject TimestampMode::fromType(JNIEnv* env, jint arg0)
    {
        return env->CallStaticObjectMethod(this->clazz, this->fromTypeMethod, arg0);
    }
}
