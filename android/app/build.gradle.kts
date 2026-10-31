val NDK_VERSION by extra(project.properties["NDK_VERSION"] as? String ?: "27.3.13750724")
val ARCH_ABI by extra(project.properties["ARCH_ABI"] as? String ?: "arm64-v8a,armeabi-v7a,x86_64")
val MIN_SDK by extra((project.properties["MIN_SDK"] as? String ?: "24").toInt())
val TARGET_SDK by extra((project.properties["TARGET_SDK"] as? String ?: "35").toInt())
val STL_TYPE by extra(project.properties["STL_TYPE"] as? String ?: "c++_shared")
val SFML_STATIC by extra(project.properties["SFML_STATIC"] as? String ?: "OFF")

plugins {
    id("com.android.application")
}

android {
    namespace = "com.blockbit.game"
    ndkVersion = NDK_VERSION
    compileSdk = TARGET_SDK

    defaultConfig {
        applicationId = "com.blockbit.game"
        minSdk = MIN_SDK
        targetSdk = TARGET_SDK
        versionCode = 1
        versionName = "0.1"

        // abiFilters is set via `splits.abi.include(...)` below so that each
        // ABI gets its own APK (plus the chosen architectures override which
        // ABIs the NDK is actually asked to produce).

        externalNativeBuild {
            cmake {
                arguments.add("-DANDROID_STL=${STL_TYPE}")
                arguments.add("-DSFML_STATIC_LIBRARIES=${SFML_STATIC}")
            }
        }
    }

    signingConfigs {
        // Release APKs still have to be signed to install via adb. Point at
        // the standard debug keystore the SDK already generates — fine for a
        // sideload-only distribution.
        create("sideload") {
            storeFile = file("${System.getProperty("user.home")}/.android/debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
    }

    buildTypes {
        getByName("debug") {
            // Keep the default: debuggable, symbols kept.
        }
        release {
            isMinifyEnabled = false
            // Strip debug symbols from the shipped .so files but keep the
            // -g binaries around in build/intermediates/ for crash symbolication.
            ndk { debugSymbolLevel = "none" }
            signingConfig = signingConfigs.getByName("sideload")
            // -O3 is already the NDK default for release builds; nothing extra.
        }
    }

    // Produce one APK per ABI instead of a single universal APK — the user
    // picks the right file for their device. Universal APK is still available
    // via -PUNIVERSAL_APK=true if someone wants a single artifact.
    splits {
        abi {
            isEnable = true
            reset()
            include(*ARCH_ABI.split(",").toTypedArray())
            isUniversalApk = project.properties["UNIVERSAL_APK"] == "true"
        }
    }

    externalNativeBuild {
        cmake {
            // Thin wrapper that `add_subdirectory`s the repo-root CMakeLists,
            // which defines libBlockbit under its if(ANDROID) branch.
            path("src/main/cpp/CMakeLists.txt")
            version = "3.28.0+"
        }
    }
}
