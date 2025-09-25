plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "nami.example.dtvbt2"
    compileSdk = 34

    defaultConfig {
        applicationId = "nami.example.dtvbt2"
        minSdk = 30
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        externalNativeBuild {
            ndkBuild {
                val gstRoot: String? = if (project.hasProperty("gstAndroidRoot")) {
                    project.findProperty("gstAndroidRoot") as String
                } else {
                    System.getenv("GSTREAMER_ROOT_ANDROID")
                }

                if (gstRoot == null) {
                    throw GradleException(
                        "GSTREAMER_ROOT_ANDROID must be set, or \"gstAndroidRoot\" must be defined in your gradle.properties in the top level directory of the unpacked universal GStreamer Android binaries"
                    )
                }

                arguments.addAll(
                    listOf(
                        "NDK_APPLICATION_MK=jni/Application.mk",
                        "GSTREAMER_JAVA_SRC_DIR=src",
                        "GSTREAMER_ROOT_ANDROID=$gstRoot",
                        "GSTREAMER_ASSETS_DIR=src/assets"
                    )
                )

                targets.add("gstreamer_jni")

                abiFilters.addAll(
                    listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
                )
            }
        }

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    kotlinOptions {
        jvmTarget = "1.8"
    }
    externalNativeBuild {
        ndkBuild {
            path = file("jni/Android.mk")
        }
    }
}

dependencies {
//    implementation("androidx.core:core-ktx:1.17.0")
    implementation("androidx.appcompat:appcompat:1.7.1")
    implementation("com.google.android.material:material:1.13.0")
    implementation("androidx.constraintlayout:constraintlayout:2.2.1")
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.3.0")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.7.0")
}