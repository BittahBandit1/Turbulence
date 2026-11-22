import SwiftUI

struct ContentView: View {
  @EnvironmentObject var model: LauncherModel
  @State private var dropTargeted = false
  @State private var showLog = false

  var body: some View {
    VStack(spacing: 18) {
      VStack(spacing: 6) {
        Text("Turbulence").font(.largeTitle.bold())
        Text("Sonic Riders, recompiled for Mac").font(.title3).foregroundStyle(.secondary)
      }

      main
        .frame(maxWidth: .infinity, maxHeight: .infinity)

      DisclosureGroup("Log", isExpanded: $showLog) {
        ScrollViewReader { proxy in
          ScrollView {
            Text(model.log.joined(separator: "\n"))
              .font(.system(.caption, design: .monospaced))
              .frame(maxWidth: .infinity, alignment: .leading)
              .textSelection(.enabled)
              .id("end")
          }
          .frame(height: 110)
          .onChange(of: model.log.count) { _ in proxy.scrollTo("end", anchor: .bottom) }
        }
      }
    }
    .padding(22)
    .frame(width: 580, height: 480)
    .dropDestination(for: URL.self) { urls, _ in
      guard let url = urls.first else { return false }
      model.accept(url)
      return true
    } isTargeted: { dropTargeted = $0 }
  }

  @ViewBuilder private var main: some View {
    switch model.phase {
    case .needsDisc:
      dropZone
    case .working(let message):
      VStack(spacing: 14) {
        ProgressView()
        Text(message).multilineTextAlignment(.center)
      }
    case .ready:
      VStack(spacing: 14) {
        Button(action: model.play) {
          Text("Play").font(.title2.bold()).frame(width: 180, height: 36)
        }
        .buttonStyle(.borderedProminent)
        .keyboardShortcut(.defaultAction)
        HStack(spacing: 16) {
          Button("Use a different disc…", action: model.changeDisc)
          Button("Show game files", action: model.revealFiles)
        }
        .buttonStyle(.link)
      }
    case .playing:
      VStack(spacing: 10) {
        ProgressView()
        Text("Sonic Riders is running. Quit the game to come back here.")
          .foregroundStyle(.secondary)
      }
    case .failed(let message):
      VStack(spacing: 12) {
        Image(systemName: "exclamationmark.triangle.fill").font(.largeTitle).foregroundStyle(.orange)
        Text(message).multilineTextAlignment(.center)
        Button("Try another disc image…", action: model.changeDisc)
      }
    case .missingTools(let clt, let cmake, let ninja):
      missingTools(clt: clt, cmake: cmake, ninja: ninja)
    }
  }

  private var dropZone: some View {
    VStack(spacing: 12) {
      Image(systemName: "opticaldisc").font(.system(size: 44)).foregroundStyle(.secondary)
      Text("Drop your Sonic Riders (USA) disc image here")
        .font(.title2)
      Text(".iso or .ciso of the GameCube disc (GXEE8P)")
        .font(.body).foregroundStyle(.secondary)
      Button("Choose File…", action: model.chooseDisc)
        .padding(.top, 4)
    }
    .frame(maxWidth: .infinity, maxHeight: .infinity)
    .background(
      RoundedRectangle(cornerRadius: 14)
        .strokeBorder(style: StrokeStyle(lineWidth: 2, dash: [7, 5]))
        .foregroundStyle(dropTargeted ? Color.accentColor : Color.secondary.opacity(0.5))
    )
    .background(
      RoundedRectangle(cornerRadius: 14)
        .fill(dropTargeted ? Color.accentColor.opacity(0.08) : Color.clear)
    )
  }

  private func missingTools(clt: Bool, cmake: Bool, ninja: Bool) -> some View {
    VStack(alignment: .leading, spacing: 10) {
      Text("A few free developer tools are needed to recompile the game (one time):")
        .font(.headline)
      if !clt {
        HStack {
          Text("• Apple Command Line Tools")
          Spacer()
          Button("Install…", action: model.installCommandLineTools)
        }
      }
      if !cmake || !ninja {
        Text("• CMake and Ninja: install Homebrew from brew.sh, then run in Terminal:")
        Text("brew install cmake ninja")
          .font(.system(.body, design: .monospaced))
          .textSelection(.enabled)
          .padding(6)
          .background(RoundedRectangle(cornerRadius: 6).fill(Color.secondary.opacity(0.15)))
      }
      HStack {
        Spacer()
        Button("Check Again", action: model.retryInstall).buttonStyle(.borderedProminent)
      }
    }
  }
}
