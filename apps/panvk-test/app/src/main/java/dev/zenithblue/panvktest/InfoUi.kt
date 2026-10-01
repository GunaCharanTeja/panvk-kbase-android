package dev.zenithblue.panvktest

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.widget.Toast
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.json.JSONArray
import org.json.JSONObject

// ============================================================================
// Data Models for Parsed Vulkan Info
// ============================================================================

data class ExtensionItem(
    val name: String,
    val specVersion: Long
)

data class FeatureItem(
    val name: String,
    val supported: Boolean
)

data class FeatureGroup(
    val key: String,
    val displayName: String,
    val features: List<FeatureItem>
) {
    val supportedCount: Int get() = features.count { it.supported }
    val totalCount: Int get() = features.size
}

data class FormatFeatureItem(
    val name: String,
    val linearFlags: List<String>,
    val optimalFlags: List<String>,
    val bufferFlags: List<String>
) {
    val hasFeatures: Boolean
        get() = linearFlags.isNotEmpty() || optimalFlags.isNotEmpty() || bufferFlags.isNotEmpty()
}

data class DeviceInfo(
    val index: Int,
    val deviceName: String,
    val apiVersion: String,
    val driverVersion: String,
    val driverName: String?,
    val driverInfo: String?,
    val conformanceVersion: String?,
    val driverId: String?,
    val vendorId: Long,
    val deviceId: Long,
    val deviceType: String,
    val extensions: List<ExtensionItem>,
    val featureGroups: List<FeatureGroup>,
    val limits: List<Pair<String, String>>,
    val formats: List<FormatFeatureItem>
)

data class ParsedVulkanInfo(
    val instanceExtensions: List<ExtensionItem>,
    val devices: List<DeviceInfo>
)

// ============================================================================
// Format Feature Flag Bit Definitions and Decoder
// ============================================================================

private val FORMAT_FEATURE_FLAGS = listOf(
    0x00000001L to "SMP",            // VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
    0x00000002L to "STOR",           // VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
    0x00000004L to "STOR_ATOM",      // VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT
    0x00000008L to "UNIF_TEX",       // VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT
    0x00000010L to "STOR_TEX",       // VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT
    0x00000020L to "STOR_TEX_ATOM",  // VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT
    0x00000040L to "VTX",            // VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT
    0x00000080L to "COLOR",          // VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
    0x00000100L to "BLEND",          // VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT
    0x00000200L to "DS",             // VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    0x00000400L to "BLIT_SRC",       // VK_FORMAT_FEATURE_BLIT_SRC_BIT
    0x00000800L to "BLIT_DST",       // VK_FORMAT_FEATURE_BLIT_DST_BIT
    0x00001000L to "SMP_LIN",        // VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
    0x00002000L to "CUBIC",          // VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT
    0x00004000L to "XFER_SRC",       // VK_FORMAT_FEATURE_TRANSFER_SRC_BIT
    0x00008000L to "XFER_DST",       // VK_FORMAT_FEATURE_TRANSFER_DST_BIT
    0x00010000L to "MINMAX",         // VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT
    0x00020000L to "MID_CHROMA",     // VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT
    0x00040000L to "YCBCR_LIN",      // VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT
    0x00080000L to "YCBCR_SEP",      // VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT
    0x00100000L to "YCBCR_EXP",      // VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT
    0x00200000L to "YCBCR_FORCE",    // VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT
    0x00400000L to "DISJOINT",       // VK_FORMAT_FEATURE_DISJOINT_BIT
    0x00800000L to "COSITED",        // VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT
    0x01000000L to "FDM",            // VK_FORMAT_FEATURE_FRAGMENT_DENSITY_MAP_BIT_EXT
    0x02000000L to "DEC_OUT",        // VK_FORMAT_FEATURE_VIDEO_DECODE_OUTPUT_BIT_KHR
    0x04000000L to "DEC_DPB",        // VK_FORMAT_FEATURE_VIDEO_DECODE_DPB_BIT_KHR
    0x08000000L to "ENC_IN",         // VK_FORMAT_FEATURE_VIDEO_ENCODE_INPUT_BIT_KHR
    0x10000000L to "ENC_DPB",        // VK_FORMAT_FEATURE_VIDEO_ENCODE_DPB_BIT_KHR
    0x20000000L to "AS_VTX",         // VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR
    0x40000000L to "FSR"             // VK_FORMAT_FEATURE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR
)

fun parseMask(v: Any?): Long {
    return when (v) {
        is Number -> v.toLong()
        is String -> {
            val s = v.trim()
            if (s.startsWith("0x", ignoreCase = true)) {
                s.substring(2).toLongOrNull(16) ?: 0L
            } else {
                s.toLongOrNull() ?: 0L
            }
        }
        else -> 0L
    }
}

fun decodeFormatFeatures(mask: Long): List<String> {
    if (mask == 0L) return emptyList()
    val res = mutableListOf<String>()
    var rem = mask
    for ((bit, label) in FORMAT_FEATURE_FLAGS) {
        if ((mask and bit) != 0L) {
            res.add(label)
            rem = rem and bit.inv()
        }
    }
    if (rem != 0L) {
        res.add("0x" + rem.toString(16).uppercase())
    }
    return res
}

// ============================================================================
// Enums / ID Decoders
// ============================================================================

fun decodeDeviceType(type: Int): String {
    return when (type) {
        0 -> "Other (0)"
        1 -> "Integrated GPU"
        2 -> "Discrete GPU"
        3 -> "Virtual GPU"
        4 -> "CPU"
        else -> if (type >= 0) "Type $type" else "N/A"
    }
}

fun decodeVendor(vendorId: Long): String {
    val hex = "0x" + vendorId.toString(16).uppercase()
    val name = when (vendorId) {
        0x13B5L, 5045L -> "ARM"
        0x10DEL, 4254L -> "NVIDIA"
        0x1002L, 4098L -> "AMD"
        0x8086L, 32902L -> "Intel"
        0x5143L, 20803L -> "Qualcomm"
        0x1010L, 4112L -> "ImgTec"
        0x14E4L -> "Broadcom"
        0x10005L -> "Mesa"
        else -> null
    }
    return if (name != null) "$name ($hex)" else hex
}

fun decodeDriverId(id: Int): String {
    return when (id) {
        1 -> "AMD Proprietary (1)"
        2 -> "AMD Open Source (2)"
        3 -> "Mesa RADV (3)"
        4 -> "NVIDIA Proprietary (4)"
        5 -> "Intel Proprietary (5)"
        6 -> "Intel Open Source Mesa (6)"
        7 -> "Imagination Proprietary (7)"
        8 -> "Qualcomm Proprietary (8)"
        9 -> "ARM Proprietary (9)"
        10 -> "Google SwiftShader (10)"
        11 -> "GGP Proprietary (11)"
        12 -> "Broadcom Proprietary (12)"
        13 -> "Mesa LLVMpipe (13)"
        14 -> "Mesa Turnip (14)"
        15 -> "Mesa V3DV (15)"
        16 -> "Mesa Panfrost (16)"
        17 -> "Mesa Venus (17)"
        18 -> "Mesa Dozen (18)"
        19 -> "Mesa NVK (19)"
        20 -> "Mesa Honeykrisp (20)"
        21 -> "Mesa Asahi (21)"
        else -> "Driver ID $id"
    }
}

private fun structOrder(name: String): Int {
    return when (name) {
        "VkPhysicalDeviceFeatures" -> 0
        "VkPhysicalDeviceVulkan11Features" -> 1
        "VkPhysicalDeviceVulkan12Features" -> 2
        "VkPhysicalDeviceVulkan13Features" -> 3
        "VkPhysicalDeviceVulkan14Features" -> 4
        else -> 10
    }
}

private fun formatStructDisplayName(name: String): String {
    return when (name) {
        "VkPhysicalDeviceFeatures" -> "Vulkan 1.0 (VkPhysicalDeviceFeatures)"
        "VkPhysicalDeviceVulkan11Features" -> "Vulkan 1.1 (VkPhysicalDeviceVulkan11Features)"
        "VkPhysicalDeviceVulkan12Features" -> "Vulkan 1.2 (VkPhysicalDeviceVulkan12Features)"
        "VkPhysicalDeviceVulkan13Features" -> "Vulkan 1.3 (VkPhysicalDeviceVulkan13Features)"
        "VkPhysicalDeviceVulkan14Features" -> "Vulkan 1.4 (VkPhysicalDeviceVulkan14Features)"
        else -> name
    }
}

fun flattenLimits(obj: JSONObject, prefix: String, result: MutableList<Pair<String, String>>) {
    val keys = obj.keys().asSequence().sorted().toList()
    for (k in keys) {
        val nextPrefix = if (prefix.isEmpty()) k else "$prefix.$k"
        val v = obj.opt(k)
        when (v) {
            is JSONObject -> {
                flattenLimits(v, nextPrefix, result)
            }
            is JSONArray -> {
                val list = mutableListOf<String>()
                for (i in 0 until v.length()) {
                    list.add(v.opt(i).toString())
                }
                result.add(nextPrefix to "[${list.joinToString(", ")}]")
            }
            JSONObject.NULL, null -> {
                result.add(nextPrefix to "null")
            }
            else -> {
                result.add(nextPrefix to v.toString())
            }
        }
    }
}

// ============================================================================
// Parser from JSONObject produced by vkinfo.c
// ============================================================================

fun parseVulkanInfo(json: JSONObject): ParsedVulkanInfo {
    // 1. Instance Extensions
    val instExts = mutableListOf<ExtensionItem>()
    val instExtArr = json.optJSONArray("instanceExtensions")
    if (instExtArr != null) {
        for (i in 0 until instExtArr.length()) {
            val obj = instExtArr.optJSONObject(i) ?: continue
            val name = obj.optString("name", "")
            val spec = obj.optLong("specVersion", 0L)
            if (name.isNotEmpty()) {
                instExts.add(ExtensionItem(name, spec))
            }
        }
    }
    instExts.sortBy { it.name }

    // 2. Devices
    val devicesArr = json.optJSONArray("devices")
    val devList = mutableListOf<DeviceInfo>()
    if (devicesArr != null) {
        for (d in 0 until devicesArr.length()) {
            val devObj = devicesArr.optJSONObject(d) ?: continue
            val props = devObj.optJSONObject("properties") ?: JSONObject()

            val devName = props.optString("deviceName", "Unknown GPU")

            // Vulkan API Version
            val apiVerRaw = props.opt("apiVersion")
            val apiVersion = when (apiVerRaw) {
                is Number -> {
                    val v = apiVerRaw.toLong()
                    "${(v shr 22) and 0x7f}.${(v shr 12) and 0x3ff}.${v and 0xfff}"
                }
                is String -> if (apiVerRaw.isNotEmpty() && apiVerRaw != "null") apiVerRaw else "Unknown"
                else -> "Unknown"
            }

            // Driver Version
            val driverVerRaw = props.opt("driverVersion")
            val driverVersion = when (driverVerRaw) {
                // Mesa packs driverVersion like VK_MAKE_VERSION(major, minor, patch)
                is Number -> driverVerRaw.toLong().let { "${it shr 22}.${(it shr 12) and 0x3FF}.${it and 0xFFF} ($it)" }
                else -> driverVerRaw?.toString()?.takeIf { it.isNotEmpty() && it != "null" } ?: "Unknown"
            }

            val driverName = props.optString("driverName").takeIf { it.isNotEmpty() && it != "null" }
            val driverInfo = props.optString("driverInfo").takeIf { it.isNotEmpty() && it != "null" }
            val conformanceVersion = props.optString("conformanceVersion").takeIf { it.isNotEmpty() && it != "null" }

            // Driver ID
            val driverIdRaw = props.opt("driverID") ?: props.opt("driverId")
            val driverId = when (driverIdRaw) {
                is Number -> decodeDriverId(driverIdRaw.toInt())
                is String -> if (driverIdRaw.isNotEmpty() && driverIdRaw != "null") driverIdRaw else null
                else -> null
            }

            val vendorId = props.optLong("vendorID", 0L)
            val deviceId = props.optLong("deviceID", 0L)
            val devTypeInt = props.optInt("deviceType", -1)
            val devTypeStr = decodeDeviceType(devTypeInt)

            // Device Extensions
            val devExts = mutableListOf<ExtensionItem>()
            val devExtArr = devObj.optJSONArray("extensions")
            if (devExtArr != null) {
                for (i in 0 until devExtArr.length()) {
                    val obj = devExtArr.optJSONObject(i) ?: continue
                    val name = obj.optString("name", "")
                    val spec = obj.optLong("specVersion", 0L)
                    if (name.isNotEmpty()) {
                        devExts.add(ExtensionItem(name, spec))
                    }
                }
            }
            devExts.sortBy { it.name }

            // Features Grouped by Struct
            val featGroups = mutableListOf<FeatureGroup>()
            val featObj = devObj.optJSONObject("features")
            if (featObj != null) {
                val keys = featObj.keys().asSequence().toList()
                val sortedKeys = keys.sortedWith { a, b ->
                    val orderA = structOrder(a)
                    val orderB = structOrder(b)
                    if (orderA != orderB) orderA.compareTo(orderB)
                    else a.compareTo(b)
                }
                for (k in sortedKeys) {
                    val subObj = featObj.optJSONObject(k) ?: continue
                    val featItems = mutableListOf<FeatureItem>()
                    val fKeys = subObj.keys().asSequence().sorted().toList()
                    for (fk in fKeys) {
                        val supported = subObj.optBoolean(fk, false)
                        featItems.add(FeatureItem(fk, supported))
                    }
                    featGroups.add(FeatureGroup(k, formatStructDisplayName(k), featItems))
                }
            }

            // Limits & Properties
            val limitsList = mutableListOf<Pair<String, String>>()
            val limitsObj = devObj.optJSONObject("limits")
            if (limitsObj != null) {
                flattenLimits(limitsObj, "", limitsList)
            }
            val sparseObj = devObj.optJSONObject("sparseProperties")
            if (sparseObj != null) {
                flattenLimits(sparseObj, "sparseProperties", limitsList)
            }
            val memoryObj = devObj.optJSONObject("memory")
            if (memoryObj != null) {
                flattenLimits(memoryObj, "memory", limitsList)
            }
            limitsList.sortBy { it.first }

            // Formats
            val formatList = mutableListOf<FormatFeatureItem>()
            val formatsObj = devObj.optJSONObject("formats")
            if (formatsObj != null) {
                val fKeys = formatsObj.keys().asSequence().sorted().toList()
                for (fk in fKeys) {
                    val fObj = formatsObj.optJSONObject(fk) ?: continue
                    val linearMask = parseMask(fObj.opt("linear"))
                    val optimalMask = parseMask(fObj.opt("optimal"))
                    val bufferMask = parseMask(fObj.opt("buffer"))
                    val linearFlags = decodeFormatFeatures(linearMask)
                    val optimalFlags = decodeFormatFeatures(optimalMask)
                    val bufferFlags = decodeFormatFeatures(bufferMask)
                    val item = FormatFeatureItem(fk, linearFlags, optimalFlags, bufferFlags)
                    if (item.hasFeatures) {
                        formatList.add(item)
                    }
                }
            }

            devList.add(
                DeviceInfo(
                    index = d,
                    deviceName = devName,
                    apiVersion = apiVersion,
                    driverVersion = driverVersion,
                    driverName = driverName,
                    driverInfo = driverInfo,
                    conformanceVersion = conformanceVersion,
                    driverId = driverId,
                    vendorId = vendorId,
                    deviceId = deviceId,
                    deviceType = devTypeStr,
                    extensions = devExts,
                    featureGroups = featGroups,
                    limits = limitsList,
                    formats = formatList
                )
            )
        }
    }

    return ParsedVulkanInfo(
        instanceExtensions = instExts,
        devices = devList
    )
}

// ============================================================================
// UI Components
// ============================================================================

@Composable
fun InfoTabContent(
    isLoading: Boolean,
    parsedInfo: ParsedVulkanInfo?,
    rawJson: String?,
    rawErrorText: String?,
    onLoadClick: () -> Unit
) {
    val context = LocalContext.current

    Column(modifier = Modifier.fillMaxSize()) {
        // Pinned Top Action Bar: Load, Copy raw JSON, Export JSON
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Button(
                enabled = !isLoading,
                onClick = onLoadClick
            ) {
                Text(if (isLoading) "Running..." else "Load")
            }

            if (rawJson != null) {
                OutlinedButton(
                    onClick = {
                        val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                        val clip = ClipData.newPlainText("Vulkan Info JSON", rawJson)
                        clipboard.setPrimaryClip(clip)
                        Toast.makeText(context, "JSON copied to clipboard", Toast.LENGTH_SHORT).show()
                    }
                ) {
                    Text("Copy JSON", maxLines = 1)
                }

                OutlinedButton(
                    onClick = {
                        val sendIntent = Intent(Intent.ACTION_SEND).apply {
                            type = "text/plain"
                            putExtra(Intent.EXTRA_TEXT, rawJson)
                        }
                        context.startActivity(Intent.createChooser(sendIntent, "Export JSON"))
                    }
                ) {
                    Text("Share", maxLines = 1)
                }
            }
        }

        Spacer(Modifier.height(8.dp))

        when {
            isLoading -> {
                Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    CircularProgressIndicator()
                }
            }
            parsedInfo != null -> {
                VulkanInfoLazyList(parsedInfo)
            }
            rawErrorText != null -> {
                SelectionContainer(
                    modifier = Modifier
                        .fillMaxSize()
                        .verticalScroll(rememberScrollState())
                ) {
                    Text(
                        text = rawErrorText,
                        fontFamily = FontFamily.Monospace,
                        style = MaterialTheme.typography.bodySmall
                    )
                }
            }
            else -> {
                Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Text(
                        text = "Tap 'Load' to query Vulkan info.",
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        }
    }
}

@Composable
fun VulkanInfoLazyList(info: ParsedVulkanInfo) {
    var selectedDeviceIndex by remember { mutableIntStateOf(0) }
    val selectedDev = info.devices.getOrNull(selectedDeviceIndex) ?: info.devices.firstOrNull()

    // Expansion states
    var instanceExtsExpanded by remember { mutableStateOf(false) }
    var deviceExtsExpanded by remember { mutableStateOf(false) }
    var featuresExpanded by remember { mutableStateOf(false) }
    var limitsExpanded by remember { mutableStateOf(false) }
    var formatsExpanded by remember { mutableStateOf(false) }

    // Feature struct sub-group expanded states
    val groupExpandedMap = remember { mutableStateMapOf<String, Boolean>() }
    var showOnlySupportedFeatures by remember { mutableStateOf(false) }

    // Filters
    var devExtFilter by remember { mutableStateOf("") }
    var limitsFilter by remember { mutableStateOf("") }
    var formatsFilter by remember { mutableStateOf("") }

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        verticalArrangement = Arrangement.spacedBy(8.dp),
        contentPadding = PaddingValues(bottom = 24.dp)
    ) {
        // Multi-device selector tabs if > 1 device
        if (info.devices.size > 1) {
            item(key = "device_selector") {
                ScrollableTabRow(
                    selectedTabIndex = selectedDeviceIndex,
                    edgePadding = 0.dp
                ) {
                    info.devices.forEachIndexed { idx, dev ->
                        Tab(
                            selected = selectedDeviceIndex == idx,
                            onClick = { selectedDeviceIndex = idx },
                            text = { Text("GPU $idx: ${dev.deviceName}") }
                        )
                    }
                }
            }
        }

        // 1. Header Card
        if (selectedDev != null) {
            item(key = "header_card_${selectedDev.index}") {
                DeviceHeaderCard(selectedDev)
            }
        } else {
            item(key = "no_devices") {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
                ) {
                    Text(
                        text = "No Vulkan physical devices found.",
                        modifier = Modifier.padding(16.dp),
                        style = MaterialTheme.typography.bodyMedium
                    )
                }
            }
        }

        // 2. Instance Extensions Section
        item(key = "header_instance_exts") {
            SectionHeaderCard(
                title = "Instance Extensions",
                count = info.instanceExtensions.size,
                isExpanded = instanceExtsExpanded,
                onToggle = { instanceExtsExpanded = !instanceExtsExpanded }
            )
        }
        if (instanceExtsExpanded) {
            if (info.instanceExtensions.isEmpty()) {
                item(key = "inst_ext_empty") {
                    Text(
                        text = "No instance extensions found.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp)
                    )
                }
            } else {
                itemsIndexed(
                    items = info.instanceExtensions,
                    key = { index, ext -> "inst_${ext.name}_$index" }
                ) { _, ext ->
                    ExtensionRow(ext)
                }
            }
        }

        // Device-specific sections (only if a device exists)
        if (selectedDev != null) {
            // 3. Device Extensions Section
            item(key = "header_device_exts") {
                SectionHeaderCard(
                    title = "Device Extensions",
                    count = selectedDev.extensions.size,
                    isExpanded = deviceExtsExpanded,
                    onToggle = { deviceExtsExpanded = !deviceExtsExpanded }
                )
            }
            if (deviceExtsExpanded) {
                item(key = "device_exts_filter") {
                    OutlinedTextField(
                        value = devExtFilter,
                        onValueChange = { devExtFilter = it },
                        label = { Text("Filter device extensions") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth()
                    )
                }
                val filteredDevExts = if (devExtFilter.isEmpty()) selectedDev.extensions
                    else selectedDev.extensions.filter { it.name.contains(devExtFilter, ignoreCase = true) }

                if (filteredDevExts.isEmpty()) {
                    item(key = "dev_ext_empty") {
                        Text(
                            text = if (devExtFilter.isEmpty()) "No device extensions found." else "No extensions matching '$devExtFilter'",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp)
                        )
                    }
                } else {
                    itemsIndexed(
                        items = filteredDevExts,
                        key = { index, ext -> "dev_${ext.name}_$index" }
                    ) { _, ext ->
                        ExtensionRow(ext)
                    }
                }
            }

            // 4. Features Section
            item(key = "header_features") {
                val totalSupp = selectedDev.featureGroups.sumOf { it.supportedCount }
                val totalAll = selectedDev.featureGroups.sumOf { it.totalCount }
                SectionHeaderCard(
                    title = "Features",
                    countDetail = "$totalSupp / $totalAll supported",
                    isExpanded = featuresExpanded,
                    onToggle = { featuresExpanded = !featuresExpanded }
                )
            }
            if (featuresExpanded) {
                item(key = "features_switch") {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 8.dp, vertical = 4.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "Show only supported features",
                            style = MaterialTheme.typography.bodyMedium,
                            fontWeight = FontWeight.Medium
                        )
                        Switch(
                            checked = showOnlySupportedFeatures,
                            onCheckedChange = { showOnlySupportedFeatures = it }
                        )
                    }
                }

                selectedDev.featureGroups.forEach { group ->
                    val visibleFeatures = if (showOnlySupportedFeatures) {
                        group.features.filter { it.supported }
                    } else {
                        group.features
                    }
                    val isGroupExpanded = groupExpandedMap[group.key] ?: false

                    item(key = "group_header_${group.key}") {
                        StructSubHeader(
                            group = group,
                            isExpanded = isGroupExpanded,
                            onToggle = { groupExpandedMap[group.key] = !isGroupExpanded }
                        )
                    }

                    if (isGroupExpanded) {
                        if (visibleFeatures.isEmpty()) {
                            item(key = "group_empty_${group.key}") {
                                Text(
                                    text = "No supported features in this struct.",
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp)
                                )
                            }
                        } else {
                            itemsIndexed(
                                items = visibleFeatures,
                                key = { index, feat -> "feat_${group.key}_${feat.name}_$index" }
                            ) { _, feat ->
                                FeatureRow(feat)
                            }
                        }
                    }
                }
            }

            // 5. Limits & Properties Section
            item(key = "header_limits") {
                SectionHeaderCard(
                    title = "Limits & Properties",
                    count = selectedDev.limits.size,
                    isExpanded = limitsExpanded,
                    onToggle = { limitsExpanded = !limitsExpanded }
                )
            }
            if (limitsExpanded) {
                item(key = "limits_filter") {
                    OutlinedTextField(
                        value = limitsFilter,
                        onValueChange = { limitsFilter = it },
                        label = { Text("Filter limits & properties") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth()
                    )
                }
                val filteredLimits = if (limitsFilter.isEmpty()) selectedDev.limits
                    else selectedDev.limits.filter {
                        it.first.contains(limitsFilter, ignoreCase = true) ||
                        it.second.contains(limitsFilter, ignoreCase = true)
                    }

                if (filteredLimits.isEmpty()) {
                    item(key = "limits_empty") {
                        Text(
                            text = if (limitsFilter.isEmpty()) "No limits found." else "No limits matching '$limitsFilter'",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp)
                        )
                    }
                } else {
                    itemsIndexed(
                        items = filteredLimits,
                        key = { index, limit -> "lim_${limit.first}_$index" }
                    ) { _, limit ->
                        LimitRow(key = limit.first, value = limit.second)
                    }
                }
            }

            // 6. Texture Formats Section
            item(key = "header_formats") {
                SectionHeaderCard(
                    title = "Texture Formats",
                    count = selectedDev.formats.size,
                    isExpanded = formatsExpanded,
                    onToggle = { formatsExpanded = !formatsExpanded }
                )
            }
            if (formatsExpanded) {
                item(key = "formats_filter") {
                    OutlinedTextField(
                        value = formatsFilter,
                        onValueChange = { formatsFilter = it },
                        label = { Text("Filter texture formats") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth()
                    )
                }
                val filteredFormats = if (formatsFilter.isEmpty()) selectedDev.formats
                    else selectedDev.formats.filter { it.name.contains(formatsFilter, ignoreCase = true) }

                if (filteredFormats.isEmpty()) {
                    item(key = "formats_empty") {
                        Text(
                            text = if (formatsFilter.isEmpty()) "No formats with supported features found." else "No formats matching '$formatsFilter'",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp)
                        )
                    }
                } else {
                    itemsIndexed(
                        items = filteredFormats,
                        key = { index, fmt -> "fmt_${fmt.name}_$index" }
                    ) { _, fmt ->
                        FormatRow(fmt)
                    }
                }
            }
        }
    }
}

// ============================================================================
// Section Header Card
// ============================================================================

@Composable
fun SectionHeaderCard(
    title: String,
    count: Int? = null,
    countDetail: String? = null,
    isExpanded: Boolean,
    onToggle: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onToggle),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f)
        )
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 14.dp, vertical = 12.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Text(
                    text = if (isExpanded) "▼" else "▶",
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.primary
                )
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold
                )
            }

            val badgeText = when {
                countDetail != null -> countDetail
                count != null -> "$count"
                else -> null
            }
            if (badgeText != null) {
                Surface(
                    shape = RoundedCornerShape(12.dp),
                    color = MaterialTheme.colorScheme.surfaceVariant
                ) {
                    Text(
                        text = badgeText,
                        style = MaterialTheme.typography.labelSmall,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(horizontal = 8.dp, vertical = 3.dp)
                    )
                }
            }
        }
    }
}

// Khronos "Feature Requirements". Names are unique across the core feature structs.
private val CORE_REQ_BASE = listOf(
    "robustBufferAccess",
    "multiview",
    "subgroupBroadcastDynamicId",
    "imagelessFramebuffer",
    "uniformBufferStandardLayout",
    "shaderSubgroupExtendedTypes",
    "separateDepthStencilLayouts",
    "hostQueryReset",
    "timelineSemaphore",
)

private val CORE_REQ_13 = listOf(
    "shaderTerminateInvocation",
    "shaderDemoteToHelperInvocation",
    "privateData",
    "pipelineCreationCacheControl",
    "synchronization2",
    "shaderZeroInitializeWorkgroupMemory",
    "robustImageAccess",
    "subgroupSizeControl",
    "computeFullSubgroups",
    "dynamicRendering",
    "shaderIntegerDotProduct",
    "maintenance4",
    "vulkanMemoryModel",
    "vulkanMemoryModelDeviceScope",
    "inlineUniformBlock",
    "bufferDeviceAddress",
)

private val CORE_REQ_14 = listOf(
    "fullDrawIndexUint32",
    "imageCubeArray",
    "independentBlend",
    "sampleRateShading",
    "drawIndirectFirstInstance",
    "depthClamp",
    "depthBiasClamp",
    "samplerAnisotropy",
    "fragmentStoresAndAtomics",
    "shaderStorageImageExtendedFormats",
    "shaderUniformBufferArrayDynamicIndexing",
    "shaderSampledImageArrayDynamicIndexing",
    "shaderStorageBufferArrayDynamicIndexing",
    "shaderStorageImageArrayDynamicIndexing",
    "shaderImageGatherExtended",
    "shaderInt16",
    "largePoints",
    "samplerYcbcrConversion",
    "storageBuffer16BitAccess",
    "variablePointers",
    "variablePointersStorageBuffer",
    "samplerMirrorClampToEdge",
    "scalarBlockLayout",
    "shaderUniformTexelBufferArrayDynamicIndexing",
    "shaderStorageTexelBufferArrayDynamicIndexing",
    "shaderInt8",
    "storageBuffer8BitAccess",
    "globalPriorityQuery",
    "shaderSubgroupRotate",
    "shaderSubgroupRotateClustered",
    "shaderFloatControls2",
    "shaderExpectAssume",
    "bresenhamLines",
    "vertexAttributeInstanceRateDivisor",
    "indexTypeUint8",
    "maintenance5",
    "pushDescriptor",
    "dynamicRenderingLocalRead",
    "maintenance6",
    "pipelineRobustness",
)

/** Missing core feature names for Vulkan 1.[minor]. Empty = met. Null = features not queried. */
fun coreRequirementGaps(dev: DeviceInfo, minor: Int): List<String>? {
    if (dev.featureGroups.isEmpty()) return null
    val enabled = buildSet {
        for (group in dev.featureGroups) {
            for (feature in group.features) {
                if (feature.supported) add(feature.name)
            }
        }
    }
    val required = buildList {
        addAll(CORE_REQ_BASE)
        if (minor >= 3) {
            addAll(CORE_REQ_13)
            if ("descriptorIndexing" in enabled) {
                add("descriptorBindingInlineUniformBlockUpdateAfterBind")
            }
        }
        if (minor >= 4) {
            addAll(CORE_REQ_14)
            if ("protectedMemory" in enabled) add("pipelineProtectedAccess")
        }
    }
    return buildList {
        for (name in required) {
            if (name !in enabled) add(name)
        }
        val parts = dev.apiVersion.split('.')
        val major = parts.getOrNull(0)?.toIntOrNull()
        val apiMinor = parts.getOrNull(1)?.toIntOrNull()
        val apiOk = major != null && apiMinor != null &&
            (major > 1 || (major == 1 && apiMinor >= minor))
        if (!apiOk) add("apiVersion < 1.$minor")
    }
}

private fun coreRequirementValue(dev: DeviceInfo, minor: Int): String {
    val gaps = coreRequirementGaps(dev, minor) ?: return "unknown (features not queried)"
    return if (gaps.isEmpty()) "met" else "missing: ${gaps.joinToString(", ")}"
}

// ============================================================================
// Header Card for Vulkan Device Properties
// ============================================================================

@Composable
fun DeviceHeaderCard(dev: DeviceInfo) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            // GPU Name and Vulkan API Version Badge
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = dev.deviceName,
                    style = MaterialTheme.typography.titleLarge,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier.weight(1f, fill = false)
                )
                Spacer(Modifier.width(8.dp))
                Surface(
                    shape = RoundedCornerShape(8.dp),
                    color = MaterialTheme.colorScheme.primary
                ) {
                    Text(
                        text = "Vulkan ${dev.apiVersion}",
                        style = MaterialTheme.typography.labelMedium,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onPrimary,
                        modifier = Modifier.padding(horizontal = 8.dp, vertical = 4.dp)
                    )
                }
            }

            HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f))

            // Driver Information
            if (dev.driverName != null || dev.driverInfo != null) {
                val driverSummary = buildString {
                    if (dev.driverName != null) append(dev.driverName)
                    if (dev.driverInfo != null) {
                        if (isNotEmpty()) append(" (")
                        append(dev.driverInfo)
                        if (dev.driverName != null) append(")")
                    }
                }
                InfoKeyVal("Driver", driverSummary)
                InfoKeyVal("Driver Version", dev.driverVersion)
            } else {
                InfoKeyVal("Driver Version", dev.driverVersion)
            }

            InfoKeyVal("Driver ID", dev.driverId ?: if (dev.driverName == "panvk") "MESA_PANVK" else dev.driverName ?: "N/A")

            val devIdHex = "0x" + dev.deviceId.toString(16).uppercase()
            InfoKeyVal("Vendor / Device ID", "${decodeVendor(dev.vendorId)} / $devIdHex")
            InfoKeyVal("Device Type", dev.deviceType)

            InfoKeyVal(
                "Conformance",
                when (dev.conformanceVersion) {
                    null -> "Not reported"
                    "0.0.0.0" -> "Not Khronos-certified (driver reports 0.0.0.0)"
                    else -> dev.conformanceVersion
                }
            )
            InfoKeyVal("Vulkan 1.3 core requirements", coreRequirementValue(dev, 3))
            if (dev.featureGroups.any { it.key == "VkPhysicalDeviceVulkan14Features" }) {
                InfoKeyVal("Vulkan 1.4 core requirements", coreRequirementValue(dev, 4))
            }
        }
    }
}

@Composable
fun InfoKeyVal(label: String, value: String) {
    Column(modifier = Modifier.fillMaxWidth()) {
        Text(
            text = label,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Text(
            text = value,
            style = MaterialTheme.typography.bodySmall,
            fontWeight = FontWeight.Medium,
            fontFamily = FontFamily.Monospace,
            modifier = Modifier.fillMaxWidth()
        )
    }
}

// ============================================================================
// Row Components
// ============================================================================

@Composable
fun ExtensionRow(ext: ExtensionItem) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 8.dp, vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = ext.name,
            style = MaterialTheme.typography.bodySmall,
            fontFamily = FontFamily.Monospace,
            modifier = Modifier.weight(1f)
        )
        Spacer(Modifier.width(8.dp))
        Surface(
            shape = RoundedCornerShape(4.dp),
            color = MaterialTheme.colorScheme.surfaceVariant
        ) {
            Text(
                text = "v${ext.specVersion}",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(horizontal = 6.dp, vertical = 2.dp)
            )
        }
    }
}

@Composable
fun StructSubHeader(
    group: FeatureGroup,
    isExpanded: Boolean,
    onToggle: () -> Unit
) {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onToggle)
            .padding(vertical = 2.dp),
        shape = RoundedCornerShape(6.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.6f)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 10.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp),
                modifier = Modifier.weight(1f, fill = false)
            ) {
                Text(
                    text = if (isExpanded) "▼" else "▶",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.primary
                )
                Text(
                    text = group.displayName,
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = FontWeight.SemiBold
                )
            }
            Spacer(Modifier.width(8.dp))
            Text(
                text = "${group.supportedCount}/${group.totalCount}",
                style = MaterialTheme.typography.labelSmall,
                color = if (group.supportedCount > 0) Color(0xFF2E7D32) else Color.Gray,
                fontWeight = FontWeight.Bold
            )
        }
    }
}

@Composable
fun FeatureRow(feature: FeatureItem) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 8.dp, vertical = 3.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = feature.name,
            style = MaterialTheme.typography.bodySmall,
            fontFamily = FontFamily.Monospace,
            modifier = Modifier.weight(1f)
        )
        Text(
            text = if (feature.supported) "✓" else "✗",
            color = if (feature.supported) Color(0xFF2E7D32) else Color.Gray,
            fontWeight = FontWeight.Bold,
            fontSize = 16.sp
        )
    }
}

@Composable
fun LimitRow(key: String, value: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 8.dp, vertical = 3.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = key,
            style = MaterialTheme.typography.bodySmall,
            modifier = Modifier.weight(0.55f)
        )
        Spacer(Modifier.width(8.dp))
        Text(
            text = value,
            style = MaterialTheme.typography.bodySmall,
            fontFamily = FontFamily.Monospace,
            fontWeight = FontWeight.Medium,
            color = MaterialTheme.colorScheme.primary,
            modifier = Modifier.weight(0.45f)
        )
    }
}

@Composable
fun FormatRow(item: FormatFeatureItem) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 2.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 10.dp, vertical = 6.dp)
        ) {
            val displayName = item.name.removePrefix("VK_FORMAT_")
            Text(
                text = displayName,
                style = MaterialTheme.typography.bodyMedium,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                color = MaterialTheme.colorScheme.onSurface
            )
            Spacer(Modifier.height(4.dp))
            if (item.optimalFlags.isNotEmpty()) {
                FormatFlagsRow(
                    label = "Optimal",
                    flags = item.optimalFlags,
                    containerColor = MaterialTheme.colorScheme.primaryContainer,
                    contentColor = MaterialTheme.colorScheme.onPrimaryContainer
                )
            }
            if (item.linearFlags.isNotEmpty()) {
                FormatFlagsRow(
                    label = "Linear",
                    flags = item.linearFlags,
                    containerColor = MaterialTheme.colorScheme.secondaryContainer,
                    contentColor = MaterialTheme.colorScheme.onSecondaryContainer
                )
            }
            if (item.bufferFlags.isNotEmpty()) {
                FormatFlagsRow(
                    label = "Buffer",
                    flags = item.bufferFlags,
                    containerColor = MaterialTheme.colorScheme.tertiaryContainer,
                    contentColor = MaterialTheme.colorScheme.onTertiaryContainer
                )
            }
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun FormatFlagsRow(
    label: String,
    flags: List<String>,
    containerColor: Color,
    contentColor: Color
) {
    if (flags.isEmpty()) return
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 2.dp),
        verticalAlignment = Alignment.Top
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            fontWeight = FontWeight.Bold,
            modifier = Modifier
                .width(55.dp)
                .padding(top = 2.dp),
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        FlowRow(
            modifier = Modifier.weight(1f),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            flags.forEach { flag ->
                Surface(
                    shape = RoundedCornerShape(4.dp),
                    color = containerColor,
                    contentColor = contentColor
                ) {
                    Text(
                        text = flag,
                        fontSize = 10.sp,
                        fontWeight = FontWeight.SemiBold,
                        modifier = Modifier.padding(horizontal = 4.dp, vertical = 2.dp)
                    )
                }
            }
        }
    }
}
