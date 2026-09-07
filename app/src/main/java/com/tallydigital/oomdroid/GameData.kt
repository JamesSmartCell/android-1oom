package com.tallydigital.oomdroid

import android.content.Context
import android.net.Uri
import android.os.Environment
import android.provider.DocumentsContract
import android.util.Log
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream

data class ImportResult(val copied: Int, val detail: String)

object GameData {
    private const val TAG = "oomdroid"

    fun appDataDir(context: Context): File = File(context.filesDir, "orion1")

    fun dataDir(context: Context): File {
        val internal = appDataDir(context)
        if (hasFonts(internal)) {
            return internal
        }
        val external = context.getExternalFilesDir(null)?.let { File(it, "orion1") }
        if (external != null && hasFonts(external)) {
            return external
        }
        for (dir in publicCandidates()) {
            if (hasFonts(dir)) {
                return dir
            }
        }
        return internal
    }

    val requiredLbx = listOf(
        "fonts.lbx",
        "v11.lbx",
        "backgrnd.lbx",
        "screens.lbx",
        "starmap.lbx",
        "planets.lbx",
        "ships.lbx",
        "names.lbx",
        "soundfx.lbx",
        "music.lbx",
    )

    fun hasGameData(context: Context): Boolean = missingRequired(dataDir(context)).isEmpty()

    fun missingRequired(dir: File): List<String> {
        if (!dir.isDirectory) {
            return requiredLbx
        }
        val have = dir.listFiles()
            ?.filter { it.isFile }
            ?.map { it.name.lowercase() }
            ?.toSet()
            ?: emptySet()
        return requiredLbx.filter { it !in have }
    }

    fun hasFonts(dir: File): Boolean {
        if (!dir.isDirectory) {
            return false
        }
        return dir.listFiles()?.any { it.isFile && it.name.equals("fonts.lbx", ignoreCase = true) } == true
    }

    fun publicCandidates(): List<File> {
        val roots = mutableListOf<File>()
        Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)?.let { roots.add(it) }
        Environment.getExternalStorageDirectory()?.let { roots.add(it) }
        val names = listOf("Orion", "orion", "orion1", "Orion1", "ORION1")
        val out = mutableListOf<File>()
        for (root in roots) {
            for (name in names) {
                out.add(File(root, name))
            }
        }
        return out
    }

    fun importFromPublicDownloads(dest: File): ImportResult {
        dest.mkdirs()
        val readable = publicCandidates().filter { it.isDirectory && it.canRead() }
        if (readable.isEmpty()) {
            return ImportResult(
                0,
                "Cannot read Download/Orion. Android blocks that folder unless you grant All files access or pick it with the folder chooser.",
            )
        }
        for (src in readable) {
            val n = copyFromFileDir(src, dest)
            if (n > 0) {
                val missing = missingRequired(dest)
                return if (missing.isEmpty()) {
                    ImportResult(n, "Copied $n files from ${src.absolutePath}")
                } else {
                    ImportResult(n, "Copied $n files, but still missing ${missing.joinToString()}. Need a full MOO1 v1.3 set (V11.LBX is the 1.3 patch).")
                }
            }
        }
        return ImportResult(
            0,
            "Looked in ${readable.joinToString { it.absolutePath }} but found no .lbx files.",
        )
    }

    fun importFromTree(context: Context, tree: Uri): ImportResult {
        val dest = appDataDir(context)
        dest.mkdirs()
        return try {
            val n = copyFromTree(context, tree, dest)
            if (n > 0) {
                val missing = missingRequired(dest)
                if (missing.isEmpty()) {
                    ImportResult(n, "Copied $n files from the folder you picked.")
                } else {
                    ImportResult(n, "Copied $n files, but still missing ${missing.joinToString()}. Need a full MOO1 v1.3 set (V11.LBX is the 1.3 patch).")
                }
            } else {
                ImportResult(0, "The folder opened, but no .lbx files were found. Pick the folder that actually contains fonts.lbx.")
            }
        } catch (e: Exception) {
            Log.e(TAG, "importFromTree", e)
            ImportResult(0, "Could not read that folder: ${e.message}")
        }
    }

    private fun copyFromFileDir(src: File, dest: File): Int {
        var n = 0
        val files = src.listFiles() ?: return 0
        for (child in files) {
            if (child.isDirectory) {
                n += copyFromFileDir(child, dest)
                continue
            }
            if (!isGameFile(child.name)) {
                continue
            }
            FileInputStream(child).use { input ->
                FileOutputStream(File(dest, child.name.lowercase())).use { output ->
                    input.copyTo(output)
                }
            }
            n++
        }
        return n
    }

    private fun copyFromTree(context: Context, tree: Uri, dest: File): Int {
        val treeId = DocumentsContract.getTreeDocumentId(tree)
        return copyChildren(context, tree, treeId, dest)
    }

    private fun copyChildren(context: Context, tree: Uri, parentDocId: String, dest: File): Int {
        val children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, parentDocId)
        val resolver = context.contentResolver
        val projection = arrayOf(
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
        )
        var n = 0
        resolver.query(children, projection, null, null, null)?.use { cursor ->
            val idCol = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DOCUMENT_ID)
            val nameCol = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DISPLAY_NAME)
            val mimeCol = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_MIME_TYPE)
            while (cursor.moveToNext()) {
                val id = cursor.getString(idCol) ?: continue
                val name = cursor.getString(nameCol) ?: continue
                val mime = cursor.getString(mimeCol) ?: ""
                if (mime == DocumentsContract.Document.MIME_TYPE_DIR) {
                    n += copyChildren(context, tree, id, dest)
                    continue
                }
                if (!isGameFile(name)) {
                    continue
                }
                val doc = DocumentsContract.buildDocumentUriUsingTree(tree, id)
                resolver.openInputStream(doc)?.use { input ->
                    FileOutputStream(File(dest, name.lowercase())).use { output ->
                        input.copyTo(output)
                    }
                    n++
                }
            }
        }
        return n
    }

    private fun isGameFile(name: String): Boolean {
        return name.endsWith(".lbx", ignoreCase = true) || name.endsWith(".pbx", ignoreCase = true)
    }
}
