// SPDX-License-Identifier: MIT
package dev.zenithblue.panvklauncher

import android.content.res.Configuration
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Check
import androidx.compose.material.icons.rounded.Close
import androidx.compose.material.icons.rounded.ContentCopy
import androidx.compose.material.icons.rounded.Lock
import androidx.compose.material.icons.rounded.Search
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties

/** Overlay controls in editor order, with the name shown in the editor. */
private val CONTROL_ROWS = listOf(
    "A" to "A", "B" to "B", "X" to "X", "Y" to "Y",
    "LB" to "LB", "RB" to "RB", "LT" to "LT", "RT" to "RT",
    "BACK" to "Back", "START" to "Start", "L3" to "L3 (left stick press)", "R3" to "R3 (right stick press)",
    "DPAD_UP" to "D-pad up", "DPAD_DOWN" to "D-pad down", "DPAD_LEFT" to "D-pad left", "DPAD_RIGHT" to "D-pad right"
)

/**
 * Full-screen controller config editor: live overlay preview (tap a control to remap it), sticks, every button.
 * [readOnly] = template: everything is shown, [onClone] makes an editable copy.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ControllerEditorScreen(
    initial: ControllerConfig,
    readOnly: Boolean,
    subtitle: String,
    onDismiss: () -> Unit,
    onSave: (ControllerConfig) -> Unit,
    onClone: (() -> Unit)? = null
) {
    var name by remember { mutableStateOf(initial.name) }
    var output by remember { mutableStateOf(initial.output) }
    val bind = remember { mutableStateMapOf<String, String>().apply { putAll(initial.bindings) } }
    val labels = remember { mutableStateMapOf<String, String>().apply { putAll(initial.labels) } }
    var left by remember { mutableStateOf(initial.leftStick) }
    var right by remember { mutableStateOf(initial.rightStick) }
    var picking by remember { mutableStateOf<String?>(null) }
    val current = initial.with(bind.toMap(), labels.filterValues { it.isNotEmpty() }, left, right, output, name.trim().ifEmpty { "custom" })

    Dialog(onDismissRequest = onDismiss, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.surface) {
            Scaffold(
                topBar = {
                    TopAppBar(
                        title = {
                            Column {
                                Text(if (readOnly) "${initial.name} (template)" else "Edit controls", maxLines = 1, overflow = TextOverflow.Ellipsis)
                                Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1)
                            }
                        },
                        navigationIcon = { IconButton(onClick = onDismiss) { Icon(Icons.Rounded.Close, contentDescription = "Close editor") } },
                        actions = {
                            if (readOnly) {
                                if (onClone != null) FilledTonalButton(onClick = onClone, modifier = Modifier.padding(end = 8.dp)) {
                                    Icon(Icons.Rounded.ContentCopy, null, Modifier.size(18.dp)); Spacer(Modifier.width(8.dp)); Text("Clone to edit")
                                }
                            } else {
                                Button(onClick = { onSave(current) }, modifier = Modifier.padding(end = 8.dp)) { Text("Save") }
                            }
                        }
                    )
                }
            ) { inner ->
                val landscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE
                val general: @Composable ColumnScope.() -> Unit = {
                    OverlayPreview(current) { id ->
                        if (readOnly) return@OverlayPreview
                        if (id == "LS" || id == "RS") return@OverlayPreview // sticks are edited in their cards
                        picking = id
                    }
                    Text(
                        if (readOnly) "Templates are read-only. Clone one to change it." else "Tap a button in the preview to change what it does.",
                        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    OutlinedTextField(
                        name, { name = it }, label = { Text("Name") }, singleLine = true, readOnly = readOnly,
                        modifier = Modifier.fillMaxWidth()
                    )
                    Text("Sends", style = MaterialTheme.typography.labelLarge)
                    SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                        val opts = listOf("keyboard" to "Keys + mouse", "gamepad" to "Xbox pad", "both" to "Both")
                        opts.forEachIndexed { i, (v, l) ->
                            SegmentedButton(
                                selected = output == v, onClick = { if (!readOnly) output = v },
                                shape = SegmentedButtonDefaults.itemShape(i, opts.size), modifier = Modifier.heightIn(min = 48.dp)
                            ) { Text(l, maxLines = 1) }
                        }
                    }
                    StickCard("Left stick", left, readOnly) { left = it }
                    StickCard("Right stick", right, readOnly) { right = it }
                }
                val buttons: @Composable ColumnScope.() -> Unit = {
                    SectionTitle("Buttons")
                    CONTROL_ROWS.forEach { (id, title) ->
                        ButtonRow(id, title, bind[id] ?: "none", labels[id] ?: "", readOnly,
                            onPick = { picking = id }, onLabel = { labels[id] = it.take(8) })
                    }
                }
                if (landscape) {
                    Row(Modifier.padding(inner).fillMaxSize().padding(horizontal = 16.dp), horizontalArrangement = Arrangement.spacedBy(20.dp)) {
                        Column(Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(bottom = 24.dp), verticalArrangement = Arrangement.spacedBy(12.dp), content = general)
                        Column(Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(bottom = 24.dp), verticalArrangement = Arrangement.spacedBy(4.dp), content = buttons)
                    }
                } else {
                    Column(
                        Modifier.padding(inner).fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = 16.dp).padding(bottom = 24.dp),
                        verticalArrangement = Arrangement.spacedBy(12.dp)
                    ) { general(); Column(verticalArrangement = Arrangement.spacedBy(4.dp), content = buttons) }
                }
            }
        }
        picking?.let { id ->
            KeyPickerDialog(CONTROL_ROWS.firstOrNull { it.first == id }?.second ?: id, bind[id] ?: "none", onDismiss = { picking = null }) {
                // A label that only echoed the old key would now lie; clear it so the overlay shows the new key.
                if (labels[id].equals(ControllerKeys.short(bind[id] ?: "none"), ignoreCase = true)) labels.remove(id)
                bind[id] = it; picking = null
            }
        }
    }
}

/** The real overlay view in preview mode, laid out for this device's landscape screen and scaled to fit. */
@Composable
private fun OverlayPreview(cfg: ControllerConfig, onTap: (String) -> Unit) {
    val dm = LocalContext.current.resources.displayMetrics
    val w = maxOf(dm.widthPixels, dm.heightPixels); val h = minOf(dm.widthPixels, dm.heightPixels)
    Surface(
        color = androidx.compose.ui.graphics.Color(0xFF2B3140), shape = RoundedCornerShape(16.dp),
        modifier = Modifier.fillMaxWidth().aspectRatio(w / h.toFloat()).semantics { contentDescription = "Controller overlay preview" }
    ) {
        AndroidView(
            factory = { c -> GamepadOverlayView(c).apply { previewW = w; previewH = h; preview = cfg; onPreviewTap = onTap } },
            update = { v -> v.preview = cfg; v.onPreviewTap = onTap },
            modifier = Modifier.fillMaxSize()
        )
    }
}

@Composable
private fun ButtonRow(id: String, title: String, binding: String, label: String, readOnly: Boolean, onPick: () -> Unit, onLabel: (String) -> Unit) {
    Row(Modifier.fillMaxWidth().heightIn(min = 56.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        Surface(color = MaterialTheme.colorScheme.secondaryContainer, shape = RoundedCornerShape(10.dp), modifier = Modifier.size(44.dp)) {
            Box(contentAlignment = Alignment.Center) {
                Text(
                    id.removePrefix("DPAD_").let { if (it.length > 3) it.take(1) else it }.let { if (id.startsWith("DPAD_")) "D·$it" else it },
                    style = MaterialTheme.typography.labelLarge, fontWeight = FontWeight.Bold, maxLines = 1
                )
            }
        }
        OutlinedButton(
            onClick = onPick, enabled = !readOnly, contentPadding = PaddingValues(horizontal = 12.dp),
            modifier = Modifier.weight(1f).heightIn(min = 48.dp).semantics { contentDescription = "$title: ${ControllerKeys.describe(binding)}. Change action" }
        ) { Text(ControllerKeys.describe(binding), maxLines = 1, overflow = TextOverflow.Ellipsis) }
        OutlinedTextField(
            label, onLabel, singleLine = true, readOnly = readOnly, label = { Text("Label") },
            placeholder = { Text(ControllerKeys.short(binding)) },
            modifier = Modifier.width(104.dp)
        )
    }
}

@Composable
private fun StickCard(title: String, s: StickConfig, readOnly: Boolean, onChange: (StickConfig) -> Unit) {
    var picking by remember { mutableStateOf<String?>(null) }
    fun copy(mode: String = s.mode, up: String = s.up, down: String = s.down, left: String = s.left, right: String = s.right,
             sens: Float = s.sensitivity, dz: Float = s.deadzone, inv: Boolean = s.invertY) =
        StickConfig(mode, up, down, left, right, sens, dz, inv)
    Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)) {
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text(title, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.SemiBold)
            SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                val opts = listOf("keys" to "4 keys", "mouse" to "Mouse look", "none" to "Off")
                opts.forEachIndexed { i, (v, l) ->
                    SegmentedButton(
                        selected = s.mode == v, onClick = { if (!readOnly) onChange(copy(mode = v)) },
                        shape = SegmentedButtonDefaults.itemShape(i, opts.size), modifier = Modifier.heightIn(min = 48.dp)
                    ) { Text(l, maxLines = 1) }
                }
            }
            when (s.mode) {
                "keys" -> {
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        listOf("WASD" to listOf("w", "a", "s", "d"), "Arrows" to listOf("Up", "Left", "Down", "Right")).forEach { (n, k) ->
                            AssistChip(onClick = { if (!readOnly) onChange(copy(up = k[0], left = k[1], down = k[2], right = k[3])) }, label = { Text(n) }, enabled = !readOnly, modifier = Modifier.heightIn(min = 48.dp))
                        }
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        listOf("Up" to s.up, "Left" to s.left, "Down" to s.down, "Right" to s.right).forEach { (dir, k) ->
                            OutlinedButton(
                                onClick = { picking = dir }, enabled = !readOnly, contentPadding = PaddingValues(horizontal = 4.dp),
                                modifier = Modifier.weight(1f).heightIn(min = 56.dp).semantics { contentDescription = "$title $dir: ${ControllerKeys.describe(k)}" }
                            ) {
                                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                    Text(dir, style = MaterialTheme.typography.labelSmall)
                                    Text(ControllerKeys.short(k).ifEmpty { "—" }, maxLines = 1, fontWeight = FontWeight.Bold)
                                }
                            }
                        }
                    }
                    SliderRow("Deadzone", s.deadzone, 0.05f..0.9f, "%.0f%%".format(s.deadzone * 100), readOnly) { onChange(copy(dz = it)) }
                }
                "mouse" -> {
                    SliderRow("Sensitivity", s.sensitivity, 0.25f..4f, "%.2fx".format(s.sensitivity), readOnly) { onChange(copy(sens = (it * 20).toInt() / 20f)) }
                    SliderRow("Deadzone", s.deadzone, 0.02f..0.5f, "%.0f%%".format(s.deadzone * 100), readOnly) { onChange(copy(dz = it)) }
                    Row(
                        Modifier.fillMaxWidth().heightIn(min = 48.dp).clickable(enabled = !readOnly) { onChange(copy(inv = !s.invertY)) },
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("Invert Y", Modifier.weight(1f))
                        Switch(checked = s.invertY, onCheckedChange = null, enabled = !readOnly)
                    }
                }
                else -> Text("Stick does nothing.", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
    picking?.let { dir ->
        val cur = when (dir) { "Up" -> s.up; "Down" -> s.down; "Left" -> s.left; else -> s.right }
        KeyPickerDialog("$title $dir", cur, onDismiss = { picking = null }) { k ->
            onChange(when (dir) { "Up" -> copy(up = k); "Down" -> copy(down = k); "Left" -> copy(left = k); else -> copy(right = k) })
            picking = null
        }
    }
}

@Composable
private fun SliderRow(label: String, value: Float, range: ClosedFloatingPointRange<Float>, shown: String, readOnly: Boolean, onChange: (Float) -> Unit) {
    Column {
        Row { Text(label, Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium); Text(shown, style = MaterialTheme.typography.labelLarge) }
        Slider(value = value.coerceIn(range), onValueChange = onChange, valueRange = range, enabled = !readOnly,
            modifier = Modifier.semantics { contentDescription = "$label $shown" })
    }
}

/** Searchable action list: None, mouse actions, then keys by group. */
@Composable
fun KeyPickerDialog(title: String, current: String, onDismiss: () -> Unit, onPick: (String) -> Unit) {
    var q by remember { mutableStateOf("") }
    val groups = remember(q) {
        val s = q.trim().lowercase()
        val all = listOf("" to listOf("none")) + ControllerKeys.GROUPS
        all.map { (g, ks) -> g to ks.filter { k -> s.isEmpty() || k.lowercase().contains(s) || ControllerKeys.describe(k).lowercase().contains(s) } }
            .filter { it.second.isNotEmpty() }
    }
    Dialog(onDismissRequest = onDismiss) {
        Surface(shape = RoundedCornerShape(24.dp), color = MaterialTheme.colorScheme.surfaceContainerHigh, modifier = Modifier.fillMaxWidth().fillMaxHeight(0.85f)) {
            Column(Modifier.padding(top = 20.dp)) {
                Text(title, style = MaterialTheme.typography.titleLarge, modifier = Modifier.padding(horizontal = 20.dp))
                Text("Now: ${ControllerKeys.describe(current)}", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(horizontal = 20.dp))
                OutlinedTextField(
                    q, { q = it }, singleLine = true, placeholder = { Text("Search keys (e.g. esc, f5, shift, wheel)") },
                    leadingIcon = { Icon(Icons.Rounded.Search, null) },
                    modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp)
                )
                LazyColumn(Modifier.weight(1f)) {
                    groups.forEach { (g, ks) ->
                        if (g.isNotEmpty()) item(key = "h$g") {
                            Text(g, style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary, modifier = Modifier.padding(start = 20.dp, top = 12.dp, bottom = 4.dp))
                        }
                        items(ks, key = { "$g/$it" }) { k ->
                            Row(
                                Modifier.fillMaxWidth().heightIn(min = 48.dp).clickable { onPick(k) }.padding(horizontal = 20.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text(ControllerKeys.describe(k), Modifier.weight(1f))
                                if (k != "none" && !k.startsWith("mouse:")) Text(k, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                                if (k == current) Icon(Icons.Rounded.Check, contentDescription = "Selected", tint = MaterialTheme.colorScheme.primary, modifier = Modifier.padding(start = 8.dp))
                            }
                        }
                    }
                }
                Row(Modifier.fillMaxWidth().padding(8.dp), horizontalArrangement = Arrangement.End) {
                    TextButton(onClick = onDismiss, modifier = Modifier.heightIn(min = 48.dp)) { Text("Cancel") }
                }
            }
        }
    }
}

/** Short description for lists: "Left: WASD · Right: mouse look · keys + mouse". */
fun ControllerConfig.summary(): String {
    fun st(s: StickConfig, left: Boolean) = when (s.mode) {
        "mouse" -> "mouse look"; "none" -> "off"
        else -> stickCaption(left).ifEmpty { listOf(s.up, s.left, s.down, s.right).joinToString("/") { ControllerKeys.short(it) } }
    }
    val out = when (output) { "gamepad" -> "Xbox pad"; "both" -> "keys + pad"; else -> "keys + mouse" }
    return "Left: ${st(leftStick, true)} · Right: ${st(rightStick, false)} · $out"
}

/**
 * Editor for the config a game uses: its own file / a library config are edited in place; a template (or the
 * automatic default) opens read-only and "Clone to edit" makes a library copy assigned to this game.
 */
@Composable
fun GameControllerEditor(shortcutId: String, gameName: String, exePath: String, onDismiss: () -> Unit) {
    val ctx = LocalContext.current
    var cfg by remember { mutableStateOf(ControllerConfig.resolve(ctx, shortcutId, exePath)) }
    // resolve() may have just attached an exe template, so read the assignment after it.
    var effective by remember { mutableStateOf(ControllerLibrary.assignment(ctx, shortcutId)) }
    val template = effective == "" || effective.startsWith("preset:")
    key(effective) {
        ControllerEditorScreen(
            initial = cfg,
            readOnly = template,
            subtitle = "For $gameName",
            onDismiss = onDismiss,
            onSave = { c ->
                if (effective == "own") ControllerConfig.save(ctx, shortcutId, c) else ControllerLibrary.save(ctx, effective, c)
                onDismiss()
            },
            onClone = {
                // Copy becomes this game's config and opens editable.
                val copy = cfg.with(name = ControllerLibrary.copyName(ctx, gameName))
                val newRef = ControllerLibrary.save(ctx, null, copy)
                ControllerLibrary.assign(ctx, shortcutId, newRef)
                cfg = copy; effective = newRef
            }
        )
    }
}
