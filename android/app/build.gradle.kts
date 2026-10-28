val NDK_VERSION by extra(project.properties["NDK_VERSION"] as? String ?: "27.3.13750724")
val ARCH_ABI by extra(project.properties["ARCH_ABI"] as? String ?: "arm64-v8a")
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

        ndk {
            abiFilters.add(ARCH_ABI)
        }

        externalNativeBuild {
            cmake {
                arguments.add("-DANDROID_STL=${STL_TYPE}")
                arguments.add("-DSFML_STATIC_LIBRARIES=${SFML_STATIC}")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
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
