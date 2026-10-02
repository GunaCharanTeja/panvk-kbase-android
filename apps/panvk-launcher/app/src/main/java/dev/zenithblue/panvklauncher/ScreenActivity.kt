package dev.zenithblue.panvklauncher

import android.app.Activity
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import android.os.Bundle
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import java.io.File
import java.io.RandomAccessFile
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.MappedByteBuffer
import java.nio.channels.FileChannel
import kotlin.math.min

class ScreenActivity : Activity(), SurfaceHolder.Callback {

    @Volatile
    private var isSurfaceValid = false
    private var renderThread: Thread? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        val surfaceView = SurfaceView(this)
        surfaceView.keepScreenOn = true
        setContentView(surfaceView)
        surfaceView.holder.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        isSurfaceValid = true
        renderThread = Thread({
            renderLoop(holder)
        }, "ScreenRenderThread").apply { start() }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        isSurfaceValid = false
        renderThread?.join(500)
        renderThread = null
    }

    override fun onDestroy() {
        super.onDestroy()
        isSurfaceValid = false
        renderThread?.join(500)
        renderThread = null
    }

    private fun renderLoop(holder: SurfaceHolder) {
        val fbFile = File(filesDir, "container/fb.bin")
        var raf: RandomAccessFile? = null
        var channel: FileChannel? = null
        var mbb: MappedByteBuffer? = null
        var mappedLength = -1L
        var lastSeq = -1
        var pixelBuffer: ByteBuffer? = null
        var bitmap: Bitmap? = null
        var lastWidth = 0
        var lastHeight = 0
        var lastFrameCount = 0

        val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            textSize = 32f
            setShadowLayer(4f, 2f, 2f, Color.BLACK)
        }
        val centerPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            textSize = 36f
            textAlign = Paint.Align.CENTER
            setShadowLayer(4f, 2f, 2f, Color.BLACK)
        }
        val bitmapPaint = Paint(Paint.FILTER_BITMAP_FLAG)

        while (isSurfaceValid) {
            try {
                val fileExists = fbFile.isFile
                val fileLen = if (fileExists) fbFile.length() else 0L

                if (!fileExists || fileLen < 32L) {
                    val r = raf
                    if (r != null) {
                        try { channel?.close(); r.close() } catch (_: Exception) {}
                        raf = null
                        channel = null
                        mbb = null
                        mappedLength = -1L
                        bitmap?.recycle()
                        bitmap = null
                        lastSeq = -1
                    }
                } else if (fileLen != mappedLength) {
                    try { channel?.close(); raf?.close() } catch (_: Exception) {}
                    try {
                        val r = RandomAccessFile(fbFile, "r")
                        val ch = r.channel
                        val map = ch.map(FileChannel.MapMode.READ_ONLY, 0, fileLen)
                        map.order(ByteOrder.LITTLE_ENDIAN)
                        raf = r
                        channel = ch
                        mbb = map
                        mappedLength = fileLen
                    } catch (_: Exception) {
                        raf = null
                        channel = null
                        mbb = null
                        mappedLength = -1L
                    }
                }

                val curMbb = mbb
                if (curMbb != null && mappedLength >= 32L) {
                    val magic = curMbb.getInt(0)
                    if (magic == 0x31424650) {
                        val seq1 = curMbb.getInt(4)
                        if ((seq1 and 1) == 0 && seq1 != lastSeq) {
                            val w = curMbb.getInt(8)
                            val h = curMbb.getInt(12)
                            val frameCount = curMbb.getInt(24)
                            val dataSize = w * h * 4
                            if (w > 0 && h > 0 && mappedLength >= 32L + dataSize) {
                                var buf = pixelBuffer
                                if (buf == null || buf.capacity() < dataSize) {
                                    buf = ByteBuffer.allocateDirect(dataSize)
                                    pixelBuffer = buf
                                }
                                buf.clear()
                                curMbb.position(32)
                                curMbb.limit(32 + dataSize)
                                buf.put(curMbb)
                                curMbb.limit(curMbb.capacity())
                                curMbb.position(0)

                                val seq2 = curMbb.getInt(4)
                                if (seq2 == seq1) {
                                    lastSeq = seq1
                                    lastWidth = w
                                    lastHeight = h
                                    lastFrameCount = frameCount

                                    var bmp = bitmap
                                    if (bmp == null || bmp.width != w || bmp.height != h) {
                                        bmp?.recycle()
                                        bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
                                        bitmap = bmp
                                    }
                                    buf.flip()
                                    bmp.copyPixelsFromBuffer(buf)
                                }
                            }
                        }
                    }
                }

                var canvas: Canvas? = null
                try {
                    canvas = holder.lockCanvas()
                    if (canvas != null) {
                        canvas.drawColor(Color.BLACK)
                        val curBmp = bitmap
                        if (curBmp != null) {
                            val cw = canvas.width.toFloat()
                            val ch = canvas.height.toFloat()
                            val bw = curBmp.width.toFloat()
                            val bh = curBmp.height.toFloat()
                            val scale = min(cw / bw, ch / bh)
                            val dw = bw * scale
                            val dh = bh * scale
                            val dx = (cw - dw) / 2f
                            val dy = (ch - dh) / 2f
                            val dstRect = RectF(dx, dy, dx + dw, dy + dh)
                            val srcRect = Rect(0, 0, curBmp.width, curBmp.height)
                            canvas.drawBitmap(curBmp, srcRect, dstRect, bitmapPaint)

                            val overlay = "${lastWidth}x${lastHeight} frame $lastFrameCount"
                            canvas.drawText(overlay, 24f, 64f, textPaint)
                        } else {
                            canvas.drawText(
                                "Waiting for frames... (fb.bin)",
                                canvas.width / 2f,
                                canvas.height / 2f,
                                centerPaint
                            )
                        }
                    }
                } finally {
                    if (canvas != null) {
                        try {
                            holder.unlockCanvasAndPost(canvas)
                        } catch (_: Exception) {}
                    }
                }
            } catch (_: Exception) {
                // Ignore transient errors
            }

            try {
                Thread.sleep(16)
            } catch (_: InterruptedException) {
                break
            }
        }

        try { channel?.close(); raf?.close() } catch (_: Exception) {}
        bitmap?.recycle()
    }
}
