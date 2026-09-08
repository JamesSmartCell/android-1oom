plugins {
    alias(libs.plugins.android.application)
}

val appVersionCode = 5
val appVersionName = "1.0.4"

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

val releaseApkFileName = "OrionMaster-$appVersionName.apk"
val releaseApkDirectory = layout.buildDirectory.dir("outputs/apk/release")

tasks.register("renameReleaseApk") {
    val apkFileName = releaseApkFileName
    val apkDirectory = releaseApkDirectory
    doLast {
        val outDir = apkDirectory.get().asFile
        if (!outDir.isDirectory) {
            return@doLast
        }
        val target = outDir.resolve(apkFileName)
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

tasks.configureEach {
    if (name == "assembleRelease") {
        finalizedBy("renameReleaseApk")
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
