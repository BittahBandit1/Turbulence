import Foundation
import AppKit

/// Locates the recompiled game, its runtime, and the compiled module, then
/// launches `moderngekko-run` with the same arguments and environment the
/// project's `launch_sonic_riders.sh` script uses.
final class LauncherModel: ObservableObject {
    @Published var gameRoot: URL?
    @Published var runnerURL: URL?
    @Published var moduleURL: URL?
    @Published var graphicsBackend: String = "Metal"
    @Published var metalHUD: Bool = true
    @Published var fpsCounter: Bool = false
    @Published var statusText: String = ""
    @Published var isRunning: Bool = false

    private var rootURL: URL?
    private var process: Process?

    var canPlay: Bool {
        gameRoot != nil && runnerURL != nil && moduleURL != nil && !isRunning
    }

    init() {
        locate()
    }

    // MARK: - Discovery

    func locate() {
        rootURL = Self.findRoot()
        let root = rootURL

        // moderngekko-run binary
        if let r = root {
            runnerURL = [
                r.appendingPathComponent("build/codex-runtime/moderngekko-run"),
                r.appendingPathComponent("bin/moderngekko-run"),
            ].first { FileManager.default.isExecutableFile(atPath: $0.path) }
        }
        if runnerURL == nil {
            // Bundled copy, if the launcher is shipped with the runtime.
            if let bundled = Bundle.main.url(forAuxiliaryExecutable: "moderngekko-run") {
                runnerURL = bundled
            }
        }

        // Compiled game module (.dylib)
        if let r = root {
            let active = r.appendingPathComponent("build/modules/GXEE8P/active-module.txt")
            if let line = try? String(contentsOf: active, encoding: .utf8)
                .split(separator: "\n").first.map(String.init) {
                let candidate = r.appendingPathComponent(line)
                if FileManager.default.fileExists(atPath: candidate.path) { moduleURL = candidate }
            }
            if moduleURL == nil {
                let candidate = r.appendingPathComponent("module/gGXEE8P_recomp.dylib")
                if FileManager.default.fileExists(atPath: candidate.path) { moduleURL = candidate }
            }
        }

        // Extracted game folder (contains sys/main.dol)
        if let r = root {
            let extracted = r.appendingPathComponent("extracted")
            if let entries = try? FileManager.default.contentsOfDirectory(
                at: extracted, includingPropertiesForKeys: nil) {
                gameRoot = entries.first { Self.isDirectory($0)
                    && FileManager.default.fileExists(atPath: $0.appendingPathComponent("sys/main.dol").path) }
            }
        }

        updateStatus()
    }

    func chooseGameFolder() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.allowsMultipleSelection = false
        panel.prompt = "Choose"
        panel.message = "Select the extracted Sonic Riders folder (the one that contains sys/main.dol)."
        guard panel.runModal() == .OK, let url = panel.url else { return }
        if FileManager.default.fileExists(atPath: url.appendingPathComponent("sys/main.dol").path) {
            gameRoot = url
            updateStatus()
        } else {
            statusText = "That folder doesn't look like an extracted disc (missing sys/main.dol)."
        }
    }

    // MARK: - Launching

    func play() {
        guard canPlay, let runner = runnerURL, let module = moduleURL, let game = gameRoot else { return }

        let userDir = (rootURL ?? runner.deletingLastPathComponent())
            .appendingPathComponent("user")
        let configDir = userDir.appendingPathComponent("Config")
        try? FileManager.default.createDirectory(at: configDir, withIntermediateDirectories: true)
        let destIni = configDir.appendingPathComponent("GCPadNew.ini")
        if !FileManager.default.fileExists(atPath: destIni.path),
           let src = rootURL?.appendingPathComponent("config/GCPadNew.ini"),
           FileManager.default.fileExists(atPath: src.path) {
            try? FileManager.default.copyItem(at: src, to: destIni)
        }

        let task = Process()
        task.executableURL = runner
        task.arguments = [
            "--game", game.path,
            "--module", module.path,
            "--user-dir", userDir.path,
            "--graphics", graphicsBackend,
            "--title", "Turbulence - Sonic Riders",
        ]

        var env = ProcessInfo.processInfo.environment
        env["MTL_HUD_ENABLED"] = metalHUD ? "1" : "0"
        if fpsCounter { env["TURBULENCE_FPS_COUNTER"] = "1" }
        if graphicsBackend == "Vulkan" {
            let bundled = rootURL?.appendingPathComponent("lib/libMoltenVK.dylib")
            if let p = bundled, FileManager.default.fileExists(atPath: p.path) {
                env["LIBVULKAN_PATH"] = p.path
            } else if FileManager.default.fileExists(atPath: "/opt/homebrew/lib/libMoltenVK.dylib") {
                env["LIBVULKAN_PATH"] = "/opt/homebrew/lib/libMoltenVK.dylib"
            }
        }
        task.environment = env

        task.terminationHandler = { [weak self] proc in
            DispatchQueue.main.async {
                self?.isRunning = false
                self?.process = nil
                self?.statusText = "Game exited (status \(proc.terminationStatus))."
            }
        }

        do {
            try task.run()
            process = task
            isRunning = true
            statusText = "Launching…"
        } catch {
            statusText = "Failed to launch: \(error.localizedDescription)"
        }
    }

    func stop() {
        if let p = process, p.isRunning {
            p.terminate()
        }
    }

    // MARK: - Helpers

    private func updateStatus() {
        if runnerURL == nil {
            statusText = "Runtime (moderngekko-run) not found — run `make tools` first."
        } else if moduleURL == nil {
            statusText = "Game module not found — run `make recompile GAME=…` first."
        } else if gameRoot == nil {
            statusText = "Game folder not found — choose it, or extract your ISO with `make extract`."
        } else {
            statusText = "Ready."
        }
    }

    static func findRoot() -> URL? {
        if let override = ProcessInfo.processInfo.environment["TURBULENCE_ROOT"], !override.isEmpty {
            return URL(fileURLWithPath: override)
        }

        var starts: [URL] = []
        if let exe = Bundle.main.executableURL {
            starts.append(exe)
        }
        starts.append(Bundle.main.bundleURL)
        starts.append(URL(fileURLWithPath: FileManager.default.currentDirectoryPath))

        for start in starts {
            var dir: URL? = start.hasDirectoryPath ? start : start.deletingLastPathComponent()
            while let d = dir {
                let ini = d.appendingPathComponent("config/GCPadNew.ini")
                let makefile = d.appendingPathComponent("Makefile")
                if FileManager.default.fileExists(atPath: ini.path),
                   FileManager.default.fileExists(atPath: makefile.path) {
                    return d
                }
                let parent = d.deletingLastPathComponent()
                if parent.path == d.path {
                    break  // reached the filesystem root
                }
                dir = parent
            }
        }
        return nil
    }

    static func isDirectory(_ url: URL) -> Bool {
        var isDir: ObjCBool = false
        return FileManager.default.fileExists(atPath: url.path, isDirectory: &isDir) && isDir.boolValue
    }
}
