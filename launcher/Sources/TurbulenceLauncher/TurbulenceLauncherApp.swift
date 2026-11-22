import SwiftUI

@main
struct TurbulenceLauncherApp: App {
  @StateObject private var model = LauncherModel()

  var body: some Scene {
    WindowGroup("Turbulence") {
      ContentView().environmentObject(model)
    }
    .windowResizability(.contentSize)
    .commands { CommandGroup(replacing: .newItem) {} }
  }
}
