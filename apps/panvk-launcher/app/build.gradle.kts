import java.net.URI
import java.security.MessageDigest

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.plugin.compose")
}

// Bundled driver: pinned released binary (src/main/assets/bundled-driver.json), downloaded at build
// time and sha256-checked; never committed. Override with -PpanvkSo=<path> (skips the pin check).
@Suppress("UNCHECKED_CAST")
val bundledDriver = groovy.json.JsonSlurper()
    .parse(file("src/main/assets/bundled-driver.json")) as Map<String, Any?>
@Suppress("UNCHECKED_CAST")
val bundledRelease = bundledDriver["release"] as Map<String, Any?>
val pinnedSha = bundledRelease["sha256"] as String
val pinnedUrl = "https://github.com/${bundledRelease["repo"]}/releases/download/${bundledRelease["tag"]}/${bundledRelease["asset"]}"
val pinnedCache = File(System.getProperty("user.home"), ".cache/panvk-launcher/${pinnedSha}.so")

fun sha256Of(f: File): String {
    val md = MessageDigest.getInstance("SHA-256")
    f.inputStream().use { ins ->
        val buf = ByteArray(1 shl 16)
        while (true) { val n = ins.read(buf); if (n < 0) break; md.update(buf, 0, n) }
    }
    return md.digest().joinToString("") { "%02x".format(it) }
}

val panvkSoProp = providers.gradleProperty("panvkSo").orNull
val panvkSoFile = if (panvkSoProp != null) file(panvkSoProp) else pinnedCache

val checkPanvkSo = tasks.register("checkPanvkSo") {
    inputs.property("panvkSoPath", panvkSoFile.absolutePath)
    inputs.property("pinnedSha", pinnedSha)
    outputs.upToDateWhen { panvkSoFile.exists() && (panvkSoProp != null || sha256Of(panvkSoFile) == pinnedSha) }
    doLast {
        if (panvkSoProp != null) {
            if (!panvkSoFile.exists()) throw GradleException("panvkSo not found: ${panvkSoFile.absolutePath}")
            return@doLast
        }
        if (!(pinnedCache.exists() && sha256Of(pinnedCache) == pinnedSha)) {
            pinnedCache.parentFile.mkdirs()
            val tmp = File(pinnedCache.parentFile, "${pinnedSha}.part")
            logger.lifecycle("Downloading bundled driver $pinnedUrl")
            URI(pinnedUrl).toURL().openStream().use { ins -> tmp.outputStream().use { ins.copyTo(it) } }
            val got = sha256Of(tmp)
            if (got != pinnedSha) {
                tmp.delete()
                throw GradleException("Bundled driver sha256 mismatch: expected $pinnedSha, got $got")
            }
            tmp.renameTo(pinnedCache)
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
        versionCode = 6
        versionName = "1.1.0"

        ndk {
            abiFilters.add("arm64-v8a")
        }
    }

    // Release APK is the debug build (debug key keeps upgrades working). AGP signs v2-only at minSdk 28;
    // some installers report "invalid package" for that, so also add v1 (JAR) and v3 signatures.
    signingConfigs.getByName("debug") {
        enableV1Signing = true
        enableV2Signing = true
        enableV3Signing = true
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
    implementation("androidx.compose.material:material-icons-extended")
    implementation("androidx.activity:activity-compose:1.13.0")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
    implementation("org.apache.commons:commons-compress:1.28.0")
    implementation("org.tukaani:xz:1.10")
    implementation("com.github.luben:zstd-jni:1.5.7-4@aar")
}

// Bundled components (rootfs, Proton, FEX, DXVK): pinned in bundled-components.json, copied from
// -PcomponentsDir (default /var/tmp/panvk/components; filled by scripts/fetch-launcher-components.sh,
// build-fex-windows.sh, build-dxvk.sh) into assets/components, sha256-checked; never committed.
// -PbundleComponents=false builds a download-only APK.
val bundleComponents = providers.gradleProperty("bundleComponents").orNull != "false"
val componentsDir = file(providers.gradleProperty("componentsDir").orNull ?: "/var/tmp/panvk/components")
@Suppress("UNCHECKED_CAST")
val bundledComponents = (groovy.json.JsonSlurper().parse(file("bundled-components.json")) as Map<String, Any?>)
    .getValue("components") as List<Map<String, String>>
val componentAssetsDir = layout.buildDirectory.dir("generated/componentAssets")

val copyComponents = tasks.register<Sync>("copyComponents") {
    into(componentAssetsDir.map { it.dir("components") })
    if (bundleComponents) {
        val files = bundledComponents.map { File(componentsDir, it["file"]!!) }
        from(files)
        doFirst {
            for (c in bundledComponents) {
                val f = File(componentsDir, c["file"]!!)
                if (!f.isFile) throw GradleException(
                    "Missing bundled component $f: run scripts/fetch-launcher-components.sh, " +
                        "build-fex-windows.sh, build-dxvk.sh (or -PbundleComponents=false)"
                )
                val got = sha256Of(f)
                if (got != c["sha256"]) throw GradleException("Bundled component $f sha256 $got != pinned ${c["sha256"]}")
            }
        }
    }
}

tasks.named("preBuild") {
    dependsOn(copyComponents)
}

android {
    sourceSets.getByName("main") {
        assets.directories.add(componentAssetsDir.get().asFile.path)
    }
    // Archives are already xz/zstd: store them so they are not compressed twice and can be openFd'd.
    androidResources {
        noCompress += listOf("txz", "wcp")
    }
}
