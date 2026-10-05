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

val bundledDriverJsonFile = File(rootDir.parentFile.parentFile, "apps/panvk-launcher/app/src/main/assets/bundled-driver.json")

val checkBundledDriverJson = tasks.register("checkBundledDriverJson") {
    inputs.property("bundledDriverJsonPath", bundledDriverJsonFile.absolutePath)
    outputs.upToDateWhen { bundledDriverJsonFile.exists() }
    doLast {
        if (!bundledDriverJsonFile.exists()) {
            throw GradleException("Bundled driver JSON not found at: ${bundledDriverJsonFile.absolutePath}")
        }
    }
}

val copyBundledDriverJson = tasks.register<Copy>("copyBundledDriverJson") {
    dependsOn(checkBundledDriverJson)
    from(bundledDriverJsonFile)
    into(file("build/generated/panvkAssets"))
}

tasks.named("preBuild") {
    dependsOn(copyPanvkSo)
    dependsOn(copyBundledDriverJson)
}

android {
    namespace = "dev.zenithblue.panvktest"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "dev.zenithblue.panvktest"
        minSdk = 29
        targetSdk = 36
        versionCode = 6
        versionName = "1.2.2"

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
        assets.directories.add("build/generated/panvkAssets")
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
}
