// SPDX-License-Identifier: MIT
package dev.zenithblue.panvklauncher

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp

/** Pick a key / mouse action per overlay control and edit labels. Saves files/controller/<shortcutId>.json. */
@Composable
fun ControllerEditorDialog(shortcutId: String, exePath: String, onDismiss: () -> Unit) {
    val ctx = LocalContext.current
    var cfg by remember { mutableStateOf(ControllerConfig.resolve(ctx, shortcutId, exePath)) }
    val bind = remember { mutableStateMapOf<String, String>().apply { putAll(cfg.bindings) } }
    val labels = remember { mutableStateMapOf<String, String>().apply { putAll(cfg.labels) } }
    var left by remember { mutableStateOf(cfg.leftStick) }
    var right by remember { mutableStateOf(cfg.rightStick) }
    var output by remember { mutableStateOf(cfg.output) }

    fun reload(c: ControllerConfig) {
        cfg = c; bind.clear(); bind.putAll(c.bindings); labels.clear(); labels.putAll(c.labels)
        left = c.leftStick; right = c.rightStick; output = c.output
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Controller") },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text("Output", style = MaterialTheme.typography.labelLarge)
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf("keyboard", "gamepad", "both").forEach { o ->
                        FilterChip(selected = output == o, onClick = { output = o }, label = { Text(o) }, modifier = Modifier.heightIn(min = 48.dp))
                    }
                }
                Text("keyboard = X11 keys + mouse (default). gamepad = virtual XInput pad.", style = MaterialTheme.typography.bodySmall)
                StickEditor("Left stick", left) { left = it }
                StickEditor("Right stick", right) { right = it }
                ControllerConfig.IDS.forEach { id ->
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        Text(id, Modifier.width(78.dp), style = MaterialTheme.typography.labelLarge)
                        KeyPicker(bind[id] ?: "none", Modifier.weight(1f)) { bind[id] = it }
                        OutlinedTextField(
                            labels[id] ?: "", { labels[id] = it }, singleLine = true, label = { Text("label") },
                            modifier = Modifier.width(96.dp)
                        )
                    }
                }
                TextButton(onClick = {
                    reload(ControllerConfig.presetFor(ctx, exePath) ?: ControllerConfig.default(ctx))
                }) { Text("Reset to preset / default") }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                ControllerConfig.save(ctx, shortcutId, cfg.with(bind.toMap(), labels.filterValues { it.isNotEmpty() }, left, right, output))
                onDismiss()
            }, modifier = Modifier.heightIn(min = 48.dp)) { Text("Save") }
        },
        dismissButton = { TextButton(onClick = onDismiss, modifier = Modifier.heightIn(min = 48.dp)) { Text("Cancel") } }
    )
}

@Composable
private fun StickEditor(title: String, s: StickConfig, onChange: (StickConfig) -> Unit) {
    Text(title, style = MaterialTheme.typography.labelLarge)
    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        listOf("keys", "mouse").forEach { m ->
            FilterChip(
                selected = s.mode == m, onClick = { onChange(StickConfig(m, s.up, s.down, s.left, s.right, s.sensitivity, s.deadzone, s.invertY)) },
                label = { Text(m) }, modifier = Modifier.heightIn(min = 48.dp).semantics { contentDescription = "$title $m" }
            )
        }
    }
    if (s.mode == "keys") {
        fun with(up: String = s.up, down: String = s.down, left: String = s.left, right: String = s.right) =
            StickConfig(s.mode, up, down, left, right, s.sensitivity, s.deadzone, s.invertY)
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            KeyPicker(s.up, Modifier.weight(1f)) { onChange(with(up = it)) }
            KeyPicker(s.left, Modifier.weight(1f)) { onChange(with(left = it)) }
            KeyPicker(s.down, Modifier.weight(1f)) { onChange(with(down = it)) }
            KeyPicker(s.right, Modifier.weight(1f)) { onChange(with(right = it)) }
        }
        Text("up / left / down / right", style = MaterialTheme.typography.bodySmall)
    } else {
        Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf(0.5f, 0.75f, 1f, 1.5f, 2f, 3f).forEach { v ->
                FilterChip(
                    selected = s.sensitivity == v,
                    onClick = { onChange(StickConfig(s.mode, s.up, s.down, s.left, s.right, v, s.deadzone, s.invertY)) },
                    label = { Text("sens $v") }, modifier = Modifier.heightIn(min = 48.dp)
                )
            }
        }
    }
}

@Composable
private fun KeyPicker(value: String, modifier: Modifier = Modifier, onPick: (String) -> Unit) {
    var open by remember { mutableStateOf(false) }
    Box(modifier) {
        OutlinedButton(
            onClick = { open = true }, contentPadding = PaddingValues(horizontal = 6.dp),
            modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp).semantics { contentDescription = "Key $value" }
        ) { Text(value, maxLines = 1) }
        DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            ControllerKeys.CHOICES.forEach { k -> DropdownMenuItem(text = { Text(k) }, onClick = { onPick(k); open = false }) }
        }
    }
}
