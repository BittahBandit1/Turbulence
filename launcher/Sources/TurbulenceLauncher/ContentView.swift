import SwiftUI

struct ContentView: View {
    @EnvironmentObject var model: LauncherModel

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            VStack(alignment: .leading, spacing: 4) {
                Text("Turbulence")
                    .font(.largeTitle.bold())
                Text("Sonic Riders — GameCube static recompilation")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }

            Divider()

            HStack {
                Text("Game folder")
                Spacer()
                Text(model.gameRoot?.path ?? "Not found")
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .foregroundStyle(.secondary)
                    .help(model.gameRoot?.path ?? "")
                Button("Choose…") { model.chooseGameFolder() }
            }

            Picker("Graphics", selection: $model.graphicsBackend) {
                Text("Metal").tag("Metal")
                Text("Vulkan").tag("Vulkan")
                Text("Null").tag("Null")
            }

            Toggle("Metal Performance HUD", isOn: $model.metalHUD)
            Toggle("FPS counter", isOn: $model.fpsCounter)

            if let runner = model.runnerURL {
                Text("Runtime: \(runner.lastPathComponent)")
                    .font(.caption)
                    .foregroundStyle(.tertiary)
            }

            Spacer()

            Text(model.statusText)
                .font(.footnote)
                .foregroundStyle(.secondary)
                .frame(maxWidth: .infinity, alignment: .leading)

            HStack {
                Button(model.isRunning ? "Running…" : "Play") {
                    model.play()
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .disabled(!model.canPlay || model.isRunning)

                if model.isRunning {
                    Button("Quit") {
                        model.stop()
                    }
                }
            }
        }
        .padding(24)
        .frame(width: 480, height: 380)
    }
}
