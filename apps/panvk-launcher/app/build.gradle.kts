plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.plugin.compose")
}

val panvkSoProp = providers.gradleProperty("panvkSo").orNull
val defaultPanvkPath = File(rootDir.parentFile.parentFile, "build/android-dxint-dist/libvulkan_panfrost.so")
val panvkSoFile = if (panvkSoProp != null) file(panvkSoProp) else defaultPanvkPath

val checkPanvkSo = tasks.register("checkPanvkSo") {
    inputs.property("panvkSoPath", panvkSoFile.absolutePath)
    outputs.upToDateWhen { panvkSoFile.exists() }
    doLast {
        if (!panvkSoFile.exists()) {
            throw GradleException("Bundled driver panvkSo not found at: ${panvkSoFile.absolutePath}. Specify -PpanvkSo=<path> or ensure default path exists.")
        }
    }
}

val copyPanvkSo = tasks.register<Copy>("copyPanvkSo") {
    dependsOn(checkPanvkSo)
    from(panvkSoFile)
    into(file("build/generated/panvkJni/arm64-v8a"))
    rename { "libvulkan_panfrost.so" }
}

tasks.named("preBuild") {
    dependsOn(copyPanvkSo)
}

android {
    namespace = "dev.zenithblue.panvklauncher"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "dev.zenithblue.panvklauncher"
        minSdk = 28
        // targetSdk 28: W^X (targetSdk>=29) blocks execve of wine/wineserver from app data; linker64 fails ("could not exec the wine loader"). Same as Winlator/GameNative legacy.
        targetSdk = 28
        versionCode = 1
        versionName = "1.0"

        ndk {
            abiFilters.add("arm64-v8a")
        }
    }

    buildTypes {
        debug {
            isDebuggable = true
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets.getByName("main") {
        jniLibs.directories.add("build/generated/panvkJni")
    }

    // targetSdk 28 stays. targetSdk>=29 W^X blocks execve of wine/wineserver
    // from app-private storage. Sideloaded executable runtime, not a Play target.
    lint {
        disable += "ExpiredTargetSdkVersion"
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2026.02.01")
    implementation(composeBom)
    implementation("androidx.compose.material3:material3")
    implementation("androidx.activity:activity-compose:1.13.0")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
    implementation("org.apache.commons:commons-compress:1.28.0")
    implementation("org.tukaani:xz:1.10")
    implementation("com.github.luben:zstd-jni:1.5.7-4@aar")
}
