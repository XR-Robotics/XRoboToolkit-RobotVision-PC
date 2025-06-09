//Console application for testing FFmpeg camera capture and video streaming functionality
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include "CameraCapture.h"
#include <stdio.h>
#include <inttypes.h>  // For format specifiers like PRIu64
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <stdexcept>  // Add this line to support std::runtime_error
#include "H264Encoder.h"
#include "H264Decoder.h"
#include "FFmpegUtils.h"
#include "H264NALUParser.h"
#include <asio.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <conio.h> // Add this line to use _kbhit() and _getch()
#include "CameraDataReceiver.h"
#include "CameraDataSender.h"
#include "NetworkVideoSource.h"
#include <fstream>
#include <sstream>
#include <iomanip>

// Add file path constant

// main function
int RunCameraCaptureTest(char* captureDevice, int resolution_width, int resolution_height, int frameRate) {
    // Global initialization (only call once)
    avdevice_register_all();
    // Set FFmpeg log level to debug mode
    //av_log_set_level(AV_LOG_DEBUG);

    // Parameter configuration
    //const char* captureDevice = "video=HP HD Camera";
    //int resolution_width = 1280; // Parameterized
    //int resolution_height = 720; // Parameterized

    // Generate resolution string using width and height
    std::string resolution = std::to_string(resolution_width) + "x" + std::to_string(resolution_height);

    const char* codec = "mjpeg";
    //const int frameRate = 30; // Use integer constant // Parameterized
    std::string framerate_str = std::to_string(frameRate);
    int frame_count = frameRate;

    // Define decoder callback function
    auto decode_callback = [](const uint8_t* data, size_t size, int width, int height) {
        printf("Decoded frame size: %zu bytes, resolution: %dx%d\n", size, width, height);
    };

    // Create H264Decoder instance
    H264Decoder h264_decoder(decode_callback);

    // Define encoder callback function
    auto encoder_callback = [&h264_decoder](const uint8_t* data, size_t size) {
        h264_decoder.decode(data, size);
    };

    // Create encoder instance
    H264Encoder h264_encoder(resolution_width, resolution_height, encoder_callback, frameRate);

    // Define frame processing callback function
    auto frame_callback = [&h264_encoder](auto y, auto u, auto v,
        auto y_sz, auto u_sz, auto v_sz,
        int w, int h, int idx) {
            printf("Processing frame %d\n", idx);
            h264_encoder.encodeFrame(y, u, v, y_sz, u_sz, v_sz);
    };

    // Create and execute camera capture object
    CameraCapture capture(
        captureDevice,
        resolution.c_str(),
        framerate_str.c_str(),
        codec,
        frame_count
    );

    // Run capture and pass callback function
    int ret = capture.run(frame_callback);

    printf("Capture completed with exit code: %d\n", ret);

    return ret;
}

int RunSimpleCameraCaptureTest(char* captureDevice, int resolution_width, int resolution_height, int frameRate) {
    // Global initialization (only call once)
    avdevice_register_all();

    // Parameter configuration
    //const char* captureDevice = "video=Orbbec Gemini 2 RGB Camera"; // Parameterized
    //int resolution_width = 1280; // Parameterized
    //int resolution_height = 720; // Parameterized

    // Generate resolution string using width and height
    std::string resolution = std::to_string(resolution_width) + "x" + std::to_string(resolution_height);

    const char* codec = "libx264"; // mjpeg
    //const int frameRate = 30; // 60 // Parameterized
    std::string framerate_str = std::to_string(frameRate);
    int frame_count = frameRate;

    // Create output file
    FILE* output_file = fopen("captured_frames.yuv", "wb");
    if (!output_file) {
        fprintf(stderr, "Failed to create output file captured_frames.yuv\n");
        return -1;
    }

    // Define frame processing callback function
    auto frame_callback = [output_file](auto y, auto u, auto v,
        auto y_sz, auto u_sz, auto v_sz,
        int w, int h, int idx) {
            // Write current frame
            fwrite(y, 1, y_sz, output_file);
            fwrite(u, 1, u_sz, output_file);
            fwrite(v, 1, v_sz, output_file);
            printf("Wrote frame %d to captured_frames.yuv\n", idx + 1);
    };

    // Create and execute camera capture object
    CameraCapture capture(
        captureDevice,
        resolution.c_str(),
        framerate_str.c_str(),
        codec,
        frame_count
    );

    // Run capture and pass callback function
    int ret = capture.run(frame_callback);

    // Close file
    fclose(output_file);
    printf("All frames have been written to captured_frames.yuv\n");

    printf("Capture completed with exit code: %d\n", ret);

    return ret;
}


int runH264TCPTransferTest(int argc, char* argv[], const std::string & ip, int port, int resolution_width, int resolution_height, int frameRate, const std::string & camera_name) {
    if (argc != 2) {
        // This usage message will be handled by the main function's printUsage
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "s") {
        // Modify to use std::ofstream to open file
        std::ofstream output_file("decoded_frames.yuv", std::ios::binary);

        auto videoSource = std::make_unique<NetworkVideoSource>();
        videoSource->start(ip.c_str(), port, [&output_file](const char* data, int size, int width, int height) {
            // Write data to file stream
            output_file.write(data, size);
            });

        // Start a thread to listen for user input
        while (true) {
            std::cout << "Press 'q' to exit the server." << std::endl;
            char ch = _getch();
            std::cout << "You pressed: " << ch << std::endl;
            if (ch == 'q' || ch == 'Q') { // Check if 'q' or 'Q' is pressed
                videoSource.reset();
                break;
            }
        }

        printf("All frames have been written to decoded_frames.yuv\n");
    }
    else if (mode == "c") {
        // Define client run function
        auto run = [&](CameraDataSender& sender) {
            // Global initialization (only call once)
            avdevice_register_all();
            // Set FFmpeg log level to debug mode
            //av_log_set_level(AV_LOG_DEBUG);

            // Parameter configuration
            //const char* captureDevice = "video=Orbbec Gemini 2 RGB Camera"; // Parameterized
            //int resolution_width = 1280; // Parameterized
            //int resolution_height = 720; // Parameterized

            // Use resolution width and height to generate resolution string
            std::string resolution = std::to_string(resolution_width) + "x" + std::to_string(resolution_height);

            const char* codec = "mjpeg";
            //const int frameRate = 60; // Use integer constant // Parameterized
            std::string framerate_str = std::to_string(frameRate);
            int frame_count = frameRate * 5;

            // Define encoder callback function
            auto encoder_callback = [&sender](const uint8_t* data, size_t size) {
                OutputDebugStringA((std::string("encode done send size: ") + std::to_string(size)).c_str());
                sender.sendData(reinterpret_cast<const char*>(data), static_cast<uint32_t>(size));
            };

            // Create encoder instance
            H264Encoder h264_encoder(resolution_width, resolution_height, encoder_callback, frameRate);

            // Define frame processing callback function
            auto frame_callback = [&h264_encoder](auto y, auto u, auto v,
                auto y_sz, auto u_sz, auto v_sz,
                int w, int h, int idx) {
                    h264_encoder.encodeFrame(y, u, v, y_sz, u_sz, v_sz);
            };

            // Create and execute camera capture object
            CameraCapture capture(
                camera_name.c_str(),
                resolution.c_str(),
                framerate_str.c_str(),
                codec,
                frame_count
            );

            // Run capture and pass callback function
            int ret = capture.run(frame_callback);

            printf("Capture completed with exit code: %d\n", ret);

            // Disconnect
            sender.disconnect();
        };

        // Create client instance and start connection
        CameraDataSender sender(ip.c_str(), port);
        sender.startConnect(run); // Pass lambda function as callback
        sender.runIOContext(); // Run io_context to handle async operations

    }
    else {
        std::cout << "Invalid parameter, please use 's' or 'c'" << std::endl;
        return 1;
    }

    return 0;
}

int runH264TCPCameraCaptureTest(int argc, char* argv[], const std::string & server_ip, int port, int resolution_width, int resolution_height, int frameRate, const std::string & camera_name) {

    //std::string server_ip = argv[2]; // Parameterized
    //std::string camera_name = (argc > 3) ? argv[3] : "video=Orbbec Gemini 2 RGB Camera"; // Parameterized

    CameraDataSender sender(server_ip.c_str(), port);
    auto run = [&](CameraDataSender& sender) {
        avdevice_register_all();

        //int resolution_width = 1920; // Parameterized
        //int resolution_height = 1080; // Parameterized
        std::string resolution = std::to_string(resolution_width) + "x" + std::to_string(resolution_height);
        const AVCodec* avcodec = avcodec_find_decoder(AV_CODEC_ID_H264);
        const char* codec = avcodec_get_name(avcodec->id);
        //const int frameRate = 30; // Parameterized
        std::string framerate_str = std::to_string(frameRate);

        auto encoder_callback = [&sender](const uint8_t* data, size_t size) {
            if (size == 0 || data == nullptr) {
                OutputDebugStringA("Encoder callback received empty data.\n");
                return;
            }

            // Implement 4-byte big-endian length header, matching Java's ByteBuffer.putInt(length)
            // The Java code sends the entire encodedData (NALU) prefixed with its length.
            // The C++ side should do the same.

            std::vector<uint8_t> packet(4 + size);

            // Write length in big-endian format
            packet[0] = (size >> 24) & 0xFF;
            packet[1] = (size >> 16) & 0xFF;
            packet[2] = (size >> 8) & 0xFF;
            packet[3] = (size) & 0xFF;

            // Copy NALU data
            std::copy(data, data + size, packet.begin() + 4);

            // Send the length-prefixed packet
            sender.sendData(reinterpret_cast<const char*>(packet.data()), static_cast<uint32_t>(packet.size()));
        };

        H264Encoder h264_encoder(resolution_width, resolution_height, encoder_callback, frameRate);

        auto frame_callback = [&h264_encoder](auto y, auto u, auto v, auto y_sz, auto u_sz, auto v_sz, int w, int h, int idx) {
            h264_encoder.encodeFrame(y, u, v, y_sz, u_sz, v_sz);
        };

        CameraCapture capture(
            camera_name.c_str(),
            resolution.c_str(),
            framerate_str.c_str(),
            codec,
            0 // continuous
        );

        std::thread input_thread([&capture]() {
            std::cout << "Press Q to stop capturing..." << std::endl;
            while (true) {
                if (_kbhit()) {
                    char ch = _getch();
                    if (ch == 'q' || ch == 'Q') {
                        capture.stopCapture();
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            });

        int ret = capture.run(frame_callback);
        input_thread.join();
        sender.disconnect();
    };

    sender.startConnect(run);
    sender.runIOContext();
    return 0;
}


int analyzeDelay(int argc, char* argv[]) {
    // Specify the input log file (modify if needed)
    std::string inputFilename = "Messages.dblog";
    std::ifstream infile(inputFilename);
    if (!infile) {
        std::cerr << "Failed to open log file: " << inputFilename << std::endl;
        return 1;
    }

    // Define stage markers for the 6 process steps.
    std::vector<std::string> stages = {
        "get frame from camera",
        "get yuv420 frame",
        "encode done send size",
        "dataCallback decode input size",
        "frameCallback decode output size",
        "presentFrame done"
    };

    // Use separate vectors to store timestamps for each stage
    std::vector<double> stage0; // get frame from camera
    std::vector<double> stage1; // get yuv420 frame
    std::vector<double> stage2; // encode done send size
    std::vector<double> stage3; // dataCallback decode input size
    std::vector<double> stage4; // frameCallback decode output size
    std::vector<double> stage5; // presentFrame done

    std::string line;
    int lineCount = 0;
    // Read log file and save timestamps based on different stage keywords
    while (std::getline(infile, line)) {
        lineCount++;
        if (line.empty()) continue;
        std::istringstream iss(line);
        int seq;
        double timeStamp;
        int pid;
        std::string procName;
        // Parse first 4 columns: sequence number, timestamp, pid, process name
        if (!(iss >> seq >> timeStamp >> pid >> procName)) {
            std::cout << "Line " << lineCount << ": Failed to parse header." << std::endl;
            continue;
        }
        // Read remaining part as log message
        std::string message;
        std::getline(iss, message);
        size_t pos = message.find_first_not_of(" ");
        if (pos != std::string::npos) {
            message = message.substr(pos);
        }

        // Save timestamps based on different stage keywords (stages do not need to appear in order)
        if (message.find(stages[0]) != std::string::npos) {
            stage0.push_back(timeStamp);
        }
        if (message.find(stages[1]) != std::string::npos) {
            stage1.push_back(timeStamp);
        }
        if (message.find(stages[2]) != std::string::npos) {
            stage2.push_back(timeStamp);
        }
        if (message.find(stages[3]) != std::string::npos) {
            stage3.push_back(timeStamp);
        }
        if (message.find(stages[4]) != std::string::npos) {
            stage4.push_back(timeStamp);
        }
        if (message.find(stages[5]) != std::string::npos) {
            stage5.push_back(timeStamp);
        }
    }
    infile.close();

    // Output log line counts for debugging
    std::cout << "Total lines read: " << lineCount << std::endl;
    std::cout << "Stage 0 (get frame from camera) count: " << stage0.size() << std::endl;
    std::cout << "Stage 1 (get yuv420 frame) count: " << stage1.size() << std::endl;
    std::cout << "Stage 2 (encode done send size) count: " << stage2.size() << std::endl;
    std::cout << "Stage 3 (dataCallback decode input size) count: " << stage3.size() << std::endl;
    std::cout << "Stage 4 (frameCallback decode output size) count: " << stage4.size() << std::endl;
    std::cout << "Stage 5 (presentFrame done) count: " << stage5.size() << std::endl;

    // Use smallest count of log lines as complete cycle count
    size_t cycles = std::min({ stage0.size(), stage1.size(), stage2.size(), stage3.size(), stage4.size(), stage5.size() });
    std::cout << "Total complete cycles: " << cycles << std::endl;

    // Open CSV file for output.
    std::string outputFilename = "output.csv";
    std::ofstream outfile(outputFilename);
    if (!outfile) {
        std::cerr << "Failed to open CSV output file." << std::endl;
        return 1;
    }

    // Write CSV header with column titles
    outfile << "Capture,Encode,tcp transfer,Decode,Present\n";

    // Calculate and output stage durations to CSV file for each cycle
    for (size_t i = 0; i < cycles; i++) {
        double d1 = (stage1[i] - stage0[i]) * 1000;
        double d2 = (stage2[i] - stage1[i]) * 1000;
        double d3 = (stage3[i] - stage2[i]) * 1000;
        double d4 = (stage4[i] - stage3[i]) * 1000;
        double d5 = (stage5[i] - stage4[i]) * 1000;
        outfile << std::fixed << std::setprecision(6)
            << d1 << "," << d2 << "," << d3 << "," << d4 << "," << d5 << "\n";

    }

    // Compute and print average stage durations
    double total_d1 = 0.0, total_d2 = 0.0, total_d3 = 0.0, total_d4 = 0.0, total_d5 = 0.0;
    for (size_t i = 0; i < cycles; i++) {
        total_d1 += (stage1[i] - stage0[i]) * 1000;
        total_d2 += (stage2[i] - stage1[i]) * 1000;
        total_d3 += (stage3[i] - stage2[i]) * 1000;
        total_d4 += (stage4[i] - stage3[i]) * 1000;
        total_d5 += (stage5[i] - stage4[i]) * 1000;
    }
    double avg_d1 = (cycles > 0) ? total_d1 / cycles : 0;
    double avg_d2 = (cycles > 0) ? total_d2 / cycles : 0;
    double avg_d3 = (cycles > 0) ? total_d3 / cycles : 0;
    double avg_d4 = (cycles > 0) ? total_d4 / cycles : 0;
    double avg_d5 = (cycles > 0) ? total_d5 / cycles : 0;
    outfile << avg_d1 << "," << avg_d2 << "," << avg_d3 << "," << avg_d4 << "," << avg_d5 << "\n";

    outfile.close();
    std::cout << "CSV file generated: " << outputFilename << std::endl;

    return 0;
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [option] [parameters]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --camera-test        Run camera capture, H.264 encoding and decoding test" << std::endl;
    std::cout << "                       Parameters: --camera <camera_name> --width <width> --height <height> --fps <fps>" << std::endl;
    std::cout << "  --simple-capture     Run simple camera capture test (save YUV file)" << std::endl;
    std::cout << "                       Parameters: --camera <camera_name> --width <width> --height <height> --fps <fps>" << std::endl;
    std::cout << "  --tcp-transfer [s|c] Run H.264 TCP transfer test" << std::endl;
    std::cout << "                       s: Server side" << std::endl;
    std::cout << "                       c: Client side" << std::endl;
    std::cout << "                       Parameters (for client): --ip <ip_address> --port <port> --camera <camera_name> --width <width> --height <height> --fps <fps>" << std::endl;
    std::cout << "                       Parameters (for server): --ip <ip_address> --port <port>" << std::endl;
    /*std::cout << "  --tcp-camera [s|c]   Run complete camera capture, H.264 encoding, TCP transfer test" << std::endl;
    std::cout << "                       s: Server side" << std::endl;
    std::cout << "                       c: Client side" << std::endl;
    std::cout << "                       Parameters (for client): --ip <ip_address> --port <port> --camera <camera_name> --width <width> --height <height> --fps <fps>" << std::endl;
    std::cout << "                       Parameters (for server): --ip <ip_address> --port <port>" << std::endl;*/
    std::cout << "  --tcp-camera c       Run complete camera capture, H.264 encoding, TCP transfer test" << std::endl;
    //std::cout << "                       s: Server side" << std::endl;
    std::cout << "                       c: Client side" << std::endl;
    std::cout << "                       Parameters (for client): --ip <ip_address> --port <port> --camera <camera_name> --width <width> --height <height> --fps <fps>" << std::endl;
    std::cout << "                       Note: The server is located in the VideoPlayer." << std::endl;
    std::cout << "  --analyze-delay      Analyze delays in video processing stages" << std::endl;
    std::cout << "Default camera: video=Integrated Webcam" << std::endl;
    std::cout << "Default IP: 127.0.0.1" << std::endl;
    std::cout << "Default Port: 12345" << std::endl;
    std::cout << "Default Width: 1280" << std::endl;
    std::cout << "Default Height: 720" << std::endl;
    std::cout << "Default FPS: 30" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string option = argv[1];

    // Default values for parameters
    std::string camera_name = "video=Integrated Webcam";
    int resolution_width = 1280;
    int resolution_height = 720;
    int frameRate = 30;
    std::string ip = "127.0.0.1";
    int port = 12345;

    // Parse command-line arguments for common parameters
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--camera" && i + 1 < argc) {
            camera_name = argv[++i];
        }
        else if (arg == "--width" && i + 1 < argc) {
            resolution_width = std::stoi(argv[++i]);
        }
        else if (arg == "--height" && i + 1 < argc) {
            resolution_height = std::stoi(argv[++i]);
        }
        else if (arg == "--fps" && i + 1 < argc) {
            frameRate = std::stoi(argv[++i]);
        }
        else if (arg == "--ip" && i + 1 < argc) {
            ip = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        }
    }

    if (option == "--camera-test") {
        return RunCameraCaptureTest(const_cast<char*>(camera_name.c_str()), resolution_width, resolution_height, frameRate);
    }
    else if (option == "--simple-capture") {
        return RunSimpleCameraCaptureTest(const_cast<char*>(camera_name.c_str()), resolution_width, resolution_height, frameRate);
    }
    else if (option == "--tcp-transfer") {
        if (argc < 3) {
            std::cout << "Error: --tcp-transfer requires [s|c] parameter" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        // Pass the mode argument (argv[2]) to runH264TCPTransferTest
        return runH264TCPTransferTest(2, argv + 1, ip, port, resolution_width, resolution_height, frameRate, camera_name);
    }
    else if (option == "--tcp-camera") {
        if (argc < 3) {
            //std::cout << "Error: --tcp-camera requires [s|c] parameter" << std::endl;
            std::cout << "Error: --tcp-camera requires c parameter" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        // Pass the mode argument (argv[2]) to runH264TCPCameraCaptureTest
        return runH264TCPCameraCaptureTest(2, argv + 1, ip, port, resolution_width, resolution_height, frameRate, camera_name);
    }
    else if (option == "--analyze-delay") {
        return analyzeDelay(argc - 1, argv + 1);
    }
    else {
        std::cout << "Error: Unknown option " << option << std::endl;
        printUsage(argv[0]);
        return 1;
    }
}