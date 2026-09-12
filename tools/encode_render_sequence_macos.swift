// Encode an ordered PNG or BMP readback sequence using macOS system frameworks.
// Use a matching Xcode compiler and SDK (without changing xcode-select).
// DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer xcrun swiftc \
//   -module-cache-path /tmp/hm3d-swift-cache tools/encode_render_sequence_macos.swift -o /tmp/encode-render-sequence
// /tmp/encode-render-sequence <frames-directory> <new-output.mp4> <fps>
import AVFoundation
import CoreGraphics
import CoreVideo
import Foundation
import ImageIO

func fail(_ message: String) -> Never {
    FileHandle.standardError.write(Data((message + "\n").utf8))
    exit(1)
}

guard CommandLine.arguments.count == 4,
      let fps = Int32(CommandLine.arguments[3]), fps > 0 else {
    fail("Usage: encode-render-sequence <frames-directory> <new-output.mp4> <fps>")
}
let files = try FileManager.default.contentsOfDirectory(
    at: URL(fileURLWithPath: CommandLine.arguments[1]),
    includingPropertiesForKeys: nil).filter { ["png", "bmp"].contains($0.pathExtension.lowercased()) }
    .sorted { $0.lastPathComponent < $1.lastPathComponent }
let output = URL(fileURLWithPath: CommandLine.arguments[2])
guard !files.isEmpty, !FileManager.default.fileExists(atPath: output.path) else {
    fail("Expected nonempty frames and a new output path")
}
func readImage(_ url: URL) -> CGImage {
    guard let source = CGImageSourceCreateWithURL(url as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
        fail("Cannot decode \(url.path)")
    }
    return image
}
let first = readImage(files[0])
let writer = try AVAssetWriter(outputURL: output, fileType: .mp4)
let input = AVAssetWriterInput(mediaType: .video, outputSettings: [
    AVVideoCodecKey: AVVideoCodecType.h264,
    AVVideoWidthKey: first.width, AVVideoHeightKey: first.height,
    AVVideoCompressionPropertiesKey: [AVVideoAverageBitRateKey: 8_000_000]
])
input.expectsMediaDataInRealTime = false
guard writer.canAdd(input) else { fail("Cannot add video input") }
writer.add(input)
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input,
    sourcePixelBufferAttributes: [
        kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32ARGB,
        kCVPixelBufferWidthKey as String: first.width,
        kCVPixelBufferHeightKey as String: first.height,
        kCVPixelBufferCGImageCompatibilityKey as String: true,
        kCVPixelBufferCGBitmapContextCompatibilityKey as String: true
    ])
guard writer.startWriting() else { fail("Cannot start writer: \(String(describing: writer.error))") }
writer.startSession(atSourceTime: .zero)
for (index, file) in files.enumerated() {
    let deadline = Date().addingTimeInterval(30)
    while !input.isReadyForMoreMediaData {
        if writer.status == .failed || Date() > deadline { fail("Video writer stalled") }
        Thread.sleep(forTimeInterval: 0.002)
    }
    autoreleasepool {
        let image = readImage(file)
        guard image.width == first.width, image.height == first.height,
              let pool = adaptor.pixelBufferPool else { fail("Frame dimensions or buffer pool invalid") }
        var allocated: CVPixelBuffer?
        guard CVPixelBufferPoolCreatePixelBuffer(nil, pool, &allocated) == kCVReturnSuccess,
              let buffer = allocated else { fail("Cannot allocate frame buffer") }
        CVPixelBufferLockBaseAddress(buffer, [])
        guard let context = CGContext(data: CVPixelBufferGetBaseAddress(buffer),
            width: image.width, height: image.height, bitsPerComponent: 8,
            bytesPerRow: CVPixelBufferGetBytesPerRow(buffer),
            space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGImageAlphaInfo.noneSkipFirst.rawValue) else { fail("Cannot draw frame") }
        context.draw(image, in: CGRect(x: 0, y: 0, width: image.width, height: image.height))
        CVPixelBufferUnlockBaseAddress(buffer, [])
        guard adaptor.append(buffer, withPresentationTime: CMTime(value: Int64(index), timescale: fps)) else {
            fail("Cannot append frame: \(String(describing: writer.error))")
        }
    }
}
input.markAsFinished()
let completed = DispatchSemaphore(value: 0)
writer.finishWriting { completed.signal() }
guard completed.wait(timeout: .now() + 30) == .success, writer.status == .completed else {
    fail("Cannot finish video: \(String(describing: writer.error))")
}
print("Encoded \(files.count) frames at \(fps) fps: \(output.path)")
