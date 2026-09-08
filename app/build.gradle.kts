plugins {
    alias(libs.plugins.android.application)
}

val appVersionCode = 3
val appVersionName = "1.0.2"

android {
    namespace = "com.tallydigital.oomdroid"
    ndkVersion = "27.2.12479018"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.tallydigital.oomdroid"
        minSdk = 27
        targetSdk = 36
        versionCode = appVersionCode
        versionName = appVersionName
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DOOM_SRC=${rootProject.projectDir.resolve("src").canonicalFile.invariantSeparatorsPath}",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
                    "-DANDROID_APP_VERSION=${appVersionName}"
                )
                cppFlags += ""
            }
        }
    }

    buildTypes {
        release {
            optimization {
                enable = false
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }
}

tasks.configureEach {
    if (name != "assembleRelease") {
        return@configureEach
    }
    doLast {
        val outDir = layout.buildDirectory.dir("outputs/apk/release").get().asFile
        val target = outDir.resolve("OrionMaster-$appVersionName.apk")
        outDir.listFiles()
            ?.filter { it.isFile && it.extension == "apk" && it.name != target.name }
            ?.forEach { src ->
                if (target.exists()) {
                    target.delete()
                }
                src.renameTo(target)
            }
    }
}

dependencies {
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.activity.ktx)
    implementation(libs.androidx.documentfile)
    implementation(libs.material)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit)
}
