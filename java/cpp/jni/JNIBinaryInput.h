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

#ifndef OPENDNP3JAVA_JNIBINARYINPUT_H
#define OPENDNP3JAVA_JNIBINARYINPUT_H

#include <jni.h>

namespace jni
{
    class BinaryInput
    {
        friend struct JCache;

        bool init(JNIEnv* env);

        public:

        // constructor methods
        jobject init3(JNIEnv* env, jboolean arg0, jbyte arg1, jlong arg2);

        private:

        jclass clazz = nullptr;

        // constructor method ids
        jmethodID init3Constructor = nullptr;
    };
}

#endif
