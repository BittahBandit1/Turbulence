// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "TurbulenceLauncher",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(
            name: "TurbulenceLauncher",
            path: "Sources/TurbulenceLauncher"
        )
    ]
)
