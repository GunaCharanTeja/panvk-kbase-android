package dev.zenithblue.panvklauncher

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.material.icons.rounded.ArrowDropDown
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.OutlinedCard
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Close
import androidx.compose.material.icons.rounded.ErrorOutline
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp

/** Scrolling page body: 16dp gutters, content capped at 760dp and centered so landscape/tablet stay readable. */
@Composable
fun PageList(
    modifier: Modifier = Modifier,
    content: LazyListScope.() -> Unit
) {
    Box(modifier = modifier.fillMaxSize(), contentAlignment = Alignment.TopCenter) {
        LazyColumn(
            modifier = Modifier
                .widthIn(max = 760.dp)
                .fillMaxSize(),
            contentPadding = PaddingValues(start = 16.dp, end = 16.dp, top = 8.dp, bottom = 24.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
            content = content
        )
    }
}

@Composable
fun SectionTitle(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text,
        style = MaterialTheme.typography.titleSmall,
        color = MaterialTheme.colorScheme.primary,
        fontWeight = FontWeight.SemiBold,
        modifier = modifier.padding(top = 8.dp, bottom = 0.dp)
    )
}

enum class Tone { Neutral, Ok, Warn, Error, Accent }

/** Small rounded status label (Installed, Bundled, Update ...). Text carries the meaning, color only reinforces it. */
@Composable
fun StatusPill(text: String, tone: Tone = Tone.Neutral, modifier: Modifier = Modifier) {
    val s = statusColors()
    val cs = MaterialTheme.colorScheme
    val (bg, fg) = when (tone) {
        Tone.Ok -> s.ok to s.onOk
        Tone.Warn -> s.warn to s.onWarn
        Tone.Error -> cs.errorContainer to cs.onErrorContainer
        Tone.Accent -> cs.primaryContainer to cs.onPrimaryContainer
        Tone.Neutral -> cs.surfaceContainerHighest to cs.onSurfaceVariant
    }
    Surface(color = bg, contentColor = fg, shape = CircleShape, modifier = modifier) {
        Text(
            text = text,
            style = MaterialTheme.typography.labelMedium,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 4.dp)
        )
    }
}

@Composable
fun EmptyState(
    icon: ImageVector,
    title: String,
    body: String,
    modifier: Modifier = Modifier,
    action: (@Composable () -> Unit)? = null
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 24.dp, vertical = 32.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Icon(
            icon,
            contentDescription = null,
            modifier = Modifier.size(56.dp),
            tint = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Text(title, style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onSurface)
        Text(
            body,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            textAlign = androidx.compose.ui.text.style.TextAlign.Center
        )
        if (action != null) {
            Spacer(Modifier.height(8.dp))
            action()
        }
    }
}

/** Label above value, used for version / path / build id lines. */
@Composable
fun LabeledValue(label: String, value: String, modifier: Modifier = Modifier, mono: Boolean = false, maxLines: Int = 2) {
    Column(modifier = modifier) {
        Text(label, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(
            value,
            style = MaterialTheme.typography.bodyMedium,
            fontFamily = if (mono) FontFamily.Monospace else null,
            maxLines = maxLines,
            overflow = TextOverflow.Ellipsis
        )
    }
}

/** Compact "label  value" row (value may have a dimmer second line). Read as one item by TalkBack. */
@Composable
fun InfoRow(label: String, value: String, sub: String = "", subIsError: Boolean = false, labelWidth: Int = 104) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = 44.dp)
            .padding(vertical = 4.dp)
            .semantics(mergeDescendants = true) {},
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            label, style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.widthIn(min = labelWidth.dp, max = labelWidth.dp)
        )
        Column(Modifier.weight(1f)) {
            Text(value, style = MaterialTheme.typography.bodyMedium, maxLines = 2, overflow = TextOverflow.Ellipsis)
            if (sub.isNotEmpty()) {
                Text(
                    sub, style = MaterialTheme.typography.bodySmall, maxLines = 2, overflow = TextOverflow.Ellipsis,
                    color = if (subIsError) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
    }
}

@Composable
fun ErrorBanner(text: String, onDismiss: (() -> Unit)? = null, modifier: Modifier = Modifier) {
    Card(
        modifier = modifier
            .fillMaxWidth()
            .semantics { contentDescription = "Error: $text" },
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.errorContainer,
            contentColor = MaterialTheme.colorScheme.onErrorContainer
        )
    ) {
        Row(
            modifier = Modifier.padding(start = 16.dp, top = 4.dp, bottom = 4.dp, end = 4.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(Icons.Rounded.ErrorOutline, contentDescription = null)
            Text(
                text,
                style = MaterialTheme.typography.bodyMedium,
                modifier = Modifier
                    .weight(1f)
                    .padding(horizontal = 12.dp, vertical = 8.dp)
            )
            if (onDismiss != null) {
                IconButton(onClick = onDismiss) { Icon(Icons.Rounded.Close, contentDescription = "Dismiss error") }
            }
        }
    }
}

/**
 * Read-only dropdown (value, label) pairs. Whole field is the 48dp+ touch target; announced as a dropdown list.
 */
@Composable
fun DropdownField(
    label: String,
    options: List<Pair<String, String>>,
    selected: String,
    onSelect: (String) -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true
) {
    var open by remember { mutableStateOf(false) }
    val current = options.firstOrNull { it.first == selected }?.second ?: selected
    Box(modifier = modifier) {
        OutlinedCard(
            onClick = { open = true },
            enabled = enabled,
            modifier = Modifier
                .fillMaxWidth()
                .semantics { contentDescription = "$label: $current. Double tap to change." }
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(start = 16.dp, end = 12.dp, top = 8.dp, bottom = 8.dp)
                    .heightIn(min = 40.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(Modifier.weight(1f)) {
                    Text(label, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Text(current, style = MaterialTheme.typography.bodyLarge, maxLines = 2, overflow = TextOverflow.Ellipsis)
                }
                Icon(Icons.Rounded.ArrowDropDown, contentDescription = null)
            }
        }
        DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            options.forEach { (value, text) ->
                DropdownMenuItem(
                    text = { Text(text) },
                    onClick = { onSelect(value); open = false },
                    modifier = Modifier.heightIn(min = 48.dp)
                )
            }
        }
    }
}

/**
 * Resolution dropdown: optional "Default (...)" entry ("" value), presets, the current custom size, and
 * "Custom…" which asks for width x height (validated by [Resolution]).
 */
@Composable
fun ResolutionField(label: String, value: String, onValue: (String) -> Unit, defaultLabel: String? = null, enabled: Boolean = true) {
    var custom by remember { mutableStateOf(false) }
    val presets = BuiltinXServer.RESOLUTIONS
    val opts = buildList {
        if (defaultLabel != null) add("" to defaultLabel)
        presets.forEach { add(it to "$it  (${Resolution.parse(it)?.let { p -> Resolution.aspect(p.first, p.second) }})") }
        if (value.isNotEmpty() && value !in presets) add(value to "$value  (custom)")
        add("__custom__" to "Custom…")
    }
    DropdownField(label, opts, value, { if (it == "__custom__") custom = true else onValue(it) }, enabled = enabled)
    if (custom) {
        val start = Resolution.parse(value) ?: (1280 to 720)
        var w by remember { mutableStateOf(start.first.toString()) }
        var h by remember { mutableStateOf(start.second.toString()) }
        val err = Resolution.error(w.toIntOrNull(), h.toIntOrNull())
        androidx.compose.material3.AlertDialog(
            onDismissRequest = { custom = false },
            title = { Text("Custom resolution") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        androidx.compose.material3.OutlinedTextField(
                            w, { w = it.filter(Char::isDigit).take(4) }, label = { Text("Width") }, singleLine = true,
                            keyboardOptions = androidx.compose.foundation.text.KeyboardOptions(keyboardType = androidx.compose.ui.text.input.KeyboardType.Number),
                            modifier = Modifier.weight(1f)
                        )
                        Text("×")
                        androidx.compose.material3.OutlinedTextField(
                            h, { h = it.filter(Char::isDigit).take(4) }, label = { Text("Height") }, singleLine = true,
                            keyboardOptions = androidx.compose.foundation.text.KeyboardOptions(keyboardType = androidx.compose.ui.text.input.KeyboardType.Number),
                            modifier = Modifier.weight(1f)
                        )
                    }
                    Text(
                        err ?: "Aspect ${Resolution.aspect(w.toInt(), h.toInt())}. The game sees this as its screen; it is scaled to fit the display.",
                        style = MaterialTheme.typography.bodySmall,
                        color = if (err != null) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Text(
                        "${Resolution.MIN_W}x${Resolution.MIN_H} to ${Resolution.MAX_W}x${Resolution.MAX_H}, even numbers.",
                        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            },
            confirmButton = {
                androidx.compose.material3.TextButton(onClick = { onValue("${w.toInt()}x${h.toInt()}"); custom = false }, enabled = err == null) { Text("Use") }
            },
            dismissButton = { androidx.compose.material3.TextButton(onClick = { custom = false }) { Text("Cancel") } }
        )
    }
}

/** Busy card: text plus a determinate bar when [progress] is known, indeterminate otherwise. */
@Composable
fun BusyCard(text: String, progress: Float? = null, modifier: Modifier = Modifier) {
    Card(
        modifier = modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.secondaryContainer)
    ) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text(text, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSecondaryContainer)
            if (progress != null) {
                LinearProgressIndicator(progress = { progress }, modifier = Modifier.fillMaxWidth())
            } else {
                LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
            }
        }
    }
}
