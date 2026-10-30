// Tekken 6 Builder: the window around build_game.sh. The person picks their own
// disc image; the script builds Tekken 6.app from it on this Mac.
import AppKit
import SwiftUI
import UniformTypeIdentifiers

@main
struct BuilderApp: App {
  @StateObject private var model = BuildModel()
  var body: some Scene {
    WindowGroup("Tekken 6 Builder") {
      BuilderView(model: model).frame(width: 520).fixedSize()
    }
    .windowResizability(.contentSize)
  }
}

@MainActor
final class BuildModel: ObservableObject {
  enum Phase { case start, needTools, building, done(URL), failed(String) }
  @Published var phase: Phase = .start
  @Published var stepText = ""
  @Published var stepIndex = 0
  @Published var stepCount = 8
  @Published var progress: Double? = nil
  @Published var log = ""
  @Published var started = Date()
  private var process: Process?
  private var iso: URL?

  func chooseDisc() {
    let panel = NSOpenPanel()
    panel.title = "Choose your Tekken 6 disc image"
    panel.message = "Choose the disc image (.iso) of your own Tekken 6 for the Xbox 360."
    panel.allowedContentTypes = [UTType(filenameExtension: "iso") ?? .data, .diskImage]
    panel.allowsMultipleSelection = false
    guard panel.runModal() == .OK, let url = panel.url else { return }
    iso = url
    run()
  }

  func installTools() {
    let p = Process()
    p.executableURL = URL(fileURLWithPath: "/usr/bin/xcode-select")
    p.arguments = ["--install"]
    try? p.run()
  }

  func retry() { if iso != nil { run() } else { phase = .start } }

  func cancel() {
    process?.terminate()
    process = nil
    phase = .start
  }

  private func run() {
    guard let iso, let res = Bundle.main.resourceURL else { return }
    phase = .building
    stepText = "Starting"
    stepIndex = 0
    progress = nil
    log = ""
    started = Date()
    let p = Process()
    p.executableURL = URL(fileURLWithPath: "/bin/zsh")
    p.arguments = [res.appendingPathComponent("build_game.sh").path, "--iso", iso.path, "--res", res.path]
    let pipe = Pipe()
    p.standardOutput = pipe
    p.standardError = pipe
    pending = Data()
    pipe.fileHandleForReading.readabilityHandler = { [weak self] h in
      let chunk = h.availableData
      guard !chunk.isEmpty else { return }
      Task { @MainActor in self?.feed(chunk) }
    }
    p.terminationHandler = { [weak self] proc in
      Task { @MainActor in
        guard let self, self.process === proc else { return }
        self.process = nil
        if case .building = self.phase {
          if proc.terminationStatus == 3 { self.phase = .needTools }
          else if proc.terminationStatus != 0 { self.phase = .failed("The build stopped unexpectedly. The log has the details.") }
        }
      }
    }
    do {
      try p.run()
      process = p
    } catch {
      phase = .failed("Couldn't start the build: \(error.localizedDescription)")
    }
  }

  private var pending = Data()
  private func feed(_ chunk: Data) {
    pending.append(chunk)
    while let nl = pending.firstIndex(of: 0x0A) {
      handle(String(decoding: pending[..<nl], as: UTF8.self))
      pending.removeSubrange(...nl)
    }
  }

  private func handle(_ line: String) {
    if line.hasPrefix("STEP ") {
      let parts = line.dropFirst(5).split(separator: " ", maxSplits: 1)
      let nums = parts.first?.split(separator: "/") ?? []
      stepIndex = Int(nums.first ?? "") ?? stepIndex
      stepCount = Int(nums.last ?? "") ?? stepCount
      stepText = parts.count > 1 ? String(parts[1]) : ""
      progress = nil
    } else if line.hasPrefix("PROGRESS ") {
      let nums = line.dropFirst(9).split(separator: "/").compactMap { Double($0) }
      if nums.count == 2, nums[1] > 0 { progress = nums[0] / nums[1] }
    } else if line.hasPrefix("DONE ") {
      phase = .done(URL(fileURLWithPath: String(line.dropFirst(5))))
    } else if line.hasPrefix("ERROR ") {
      phase = .failed(String(line.dropFirst(6)))
    } else if line == "NEEDTOOLS" {
      phase = .needTools
    }
    log += line + "\n"
    if log.count > 400_000 { log = String(log.suffix(300_000)) }
  }
}

struct BuilderView: View {
  @ObservedObject var model: BuildModel
  @State private var showLog = false

  var body: some View {
    VStack(alignment: .leading, spacing: 14) {
      Text("Tekken 6 for Mac").font(.title2).bold()
      switch model.phase {
      case .start:
        Text("This builds Tekken 6 on your Mac from your own copy of the game: the Xbox 360 disc image (.iso) of Tekken 6, USA/Europe, Rev 1.")
        Text("It takes a few minutes on a recent Mac, longer on older ones, and needs about 10 GB of free space. The game goes in ~/Library/Application Support/Tekken 6 and the app in Applications.")
          .foregroundStyle(.secondary)
        HStack { Spacer(); Button("Choose Disc Image…") { model.chooseDisc() }.keyboardShortcut(.defaultAction) }
      case .needTools:
        Text("Building needs Apple's free Command Line Tools. Click Install, follow Apple's installer, then click Try Again.")
        HStack {
          Spacer()
          Button("Install") { model.installTools() }
          Button("Try Again") { model.retry() }.keyboardShortcut(.defaultAction)
        }
      case .building:
        Text("Step \(model.stepIndex) of \(model.stepCount): \(model.stepText)")
        if let p = model.progress { ProgressView(value: p) } else { ProgressView().progressViewStyle(.linear) }
        TimelineView(.periodic(from: .now, by: 1)) { ctx in
          Text("Elapsed \(Self.elapsed(from: model.started, to: ctx.date))").foregroundStyle(.secondary).monospacedDigit()
        }
        HStack { Toggle("Show log", isOn: $showLog); Spacer(); Button("Cancel") { model.cancel() } }
      case .done(let app):
        Text("Done. Tekken 6 is in \(app.deletingLastPathComponent().path).")
        Text("Keyboard: Return is Start, Space is A, Backspace is B, L is X, P is Y, WASD moves. A game controller works too.")
          .foregroundStyle(.secondary)
        HStack {
          Spacer()
          Button("Show in Finder") { NSWorkspace.shared.activateFileViewerSelecting([app]) }
          Button("Open Tekken 6") {
            NSWorkspace.shared.openApplication(at: app, configuration: .init())
            NSApp.terminate(nil)
          }.keyboardShortcut(.defaultAction)
        }
      case .failed(let message):
        Text(message)
        HStack {
          Toggle("Show log", isOn: $showLog)
          Spacer()
          Button("Copy Log") { NSPasteboard.general.clearContents(); NSPasteboard.general.setString(model.log, forType: .string) }
          Button("Try Again") { model.retry() }.keyboardShortcut(.defaultAction)
        }
      }
      if showLog {
        ScrollViewReader { proxy in
          ScrollView {
            Text(model.log).font(.system(.caption, design: .monospaced)).frame(maxWidth: .infinity, alignment: .leading)
              .textSelection(.enabled)
            Color.clear.frame(height: 1).id("end")
          }
          .frame(height: 220)
          .onChange(of: model.log) { _ in proxy.scrollTo("end") }
        }
      }
    }
    .padding(20)
  }

  static func elapsed(from: Date, to: Date) -> String {
    let s = max(0, Int(to.timeIntervalSince(from)))
    return String(format: "%d:%02d", s / 60, s % 60)
  }
}
