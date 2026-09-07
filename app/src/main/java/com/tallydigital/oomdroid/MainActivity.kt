package com.tallydigital.oomdroid

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.DocumentsContract
import android.provider.Settings
import android.view.KeyEvent
import android.view.View
import android.widget.Button
import android.widget.TextView
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import kotlin.concurrent.thread

class MainActivity : AppCompatActivity() {
    private lateinit var gameView: GameView
    private lateinit var setupPanel: View
    private lateinit var loadingLabel: TextView
    private lateinit var setupStatus: TextView

    private val audio = GameAudio()
    private var gameStarted = false

    private val folderPicker = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree(),
    ) { uri: Uri? ->
        if (uri == null) {
            return@registerForActivityResult
        }
        try {
            contentResolver.takePersistableUriPermission(
                uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION,
            )
        } catch (_: SecurityException) {
        }
        copyInBackground { GameData.importFromTree(this, uri) }
    }

    private val readStorage = registerForActivityResult(
        ActivityResultContracts.RequestPermission(),
    ) { granted ->
        if (granted) {
            openFolderPicker()
        } else {
            setupStatus.setText(R.string.setup_need_access)
        }
    }

    private val allFilesAccess = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult(),
    ) {
        if (hasStorageAccess()) {
            openFolderPicker()
        } else {
            setupStatus.setText(R.string.setup_need_access)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        hideSystemUi()
        setContentView(R.layout.activity_main)
        gameView = findViewById(R.id.gameView)
        setupPanel = findViewById(R.id.setupPanel)
        loadingLabel = findViewById(R.id.loadingLabel)
        setupStatus = findViewById(R.id.setupStatus)
        findViewById<Button>(R.id.setupPick).setOnClickListener {
            chooseLbxFolder()
        }
        gameView.onFirstFrame = {
            runOnUiThread { loadingLabel.visibility = View.GONE }
        }
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if (gameStarted) {
                    GameNative.onKey(27, 27, true)
                    GameNative.onKey(27, 27, false)
                } else {
                    finish()
                }
            }
        })
        if (GameData.hasGameData(this)) {
            startGame()
        }
    }

    private fun chooseLbxFolder() {
        if (hasStorageAccess()) {
            openFolderPicker()
            return
        }
        requestStorageAccess()
    }

    private fun openFolderPicker() {
        folderPicker.launch(downloadsTreeHint())
    }

    private fun downloadsTreeHint(): Uri {
        return DocumentsContract.buildDocumentUri(
            "com.android.externalstorage.documents",
            "primary:Download",
        )
    }

    private fun hasStorageAccess(): Boolean {
        if (Build.VERSION.SDK_INT >= 30) {
            return Environment.isExternalStorageManager()
        }
        return ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.READ_EXTERNAL_STORAGE,
        ) == PackageManager.PERMISSION_GRANTED
    }

    private fun requestStorageAccess() {
        setupStatus.setText(R.string.setup_need_access)
        if (Build.VERSION.SDK_INT >= 30) {
            val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
            intent.data = Uri.parse("package:$packageName")
            allFilesAccess.launch(intent)
            return
        }
        readStorage.launch(Manifest.permission.READ_EXTERNAL_STORAGE)
    }

    private fun copyInBackground(action: () -> ImportResult) {
        setupStatus.setText(R.string.setup_copying)
        thread {
            val result = action()
            runOnUiThread {
                setupStatus.text = result.detail
                if (GameData.hasGameData(this)) {
                    startGame()
                } else if (result.copied > 0) {
                    setupStatus.text = result.detail
                }
            }
        }
    }

    private fun startGame() {
        if (gameStarted) {
            return
        }
        gameStarted = true
        GameNative.videoSize()
        setupPanel.visibility = View.GONE
        gameView.visibility = View.VISIBLE
        loadingLabel.visibility = View.VISIBLE
        val data = GameData.dataDir(this).absolutePath
        audio.start()
        gameView.tick()
        thread(name = "oom-game") {
            val rc = GameNative.start(data, data)
            runOnUiThread {
                loadingLabel.visibility = View.VISIBLE
                loadingLabel.text = if (rc == 0) {
                    getString(R.string.game_loading)
                } else {
                    getString(R.string.game_failed)
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        if (gameStarted) {
            audio.resume()
        }
        hideSystemUi()
    }

    override fun onPause() {
        if (gameStarted) {
            audio.pause()
        }
        super.onPause()
    }

    override fun onDestroy() {
        audio.stop()
        super.onDestroy()
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            GameNative.onKey(27, 27, true)
            return true
        }
        val mapped = mapKey(keyCode, event)
        if (mapped != 0) {
            GameNative.onKey(mapped, event.unicodeChar and 0xff, true)
            return true
        }
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            GameNative.onKey(27, 27, false)
            return true
        }
        val mapped = mapKey(keyCode, event)
        if (mapped != 0) {
            GameNative.onKey(mapped, event.unicodeChar and 0xff, false)
            return true
        }
        return super.onKeyUp(keyCode, event)
    }

    private fun hideSystemUi() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    private fun mapKey(keyCode: Int, event: KeyEvent): Int {
        val ch = event.unicodeChar and 0xff
        if (ch in 32..126) {
            return if (ch in 65..90) ch + 32 else ch
        }
        return when (keyCode) {
            KeyEvent.KEYCODE_ENTER -> 13
            KeyEvent.KEYCODE_ESCAPE -> 27
            KeyEvent.KEYCODE_DEL -> 8
            KeyEvent.KEYCODE_SPACE -> 32
            else -> 0
        }
    }
}
