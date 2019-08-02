#include <iostream>
#include <algorithm>
#include <fstream>
#include <vector>
#include <thread>

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/filesystem.hpp>

#include "client_http.hpp"
#include "server_http.hpp"

using namespace cv;
using namespace boost::property_tree;

using std::cout;
using std::cerr;
using std::endl;
using std::shared_ptr;
using std::stringstream;
using std::thread;
using std::ifstream;
using std::vector;
using std::streamsize;
using std::ios;

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;


void readAndSendData(const shared_ptr<HttpServer::Response>& response, const shared_ptr<ifstream>& ifs);

void processImageParam(stringstream& stream);
void detectAndSave(Mat image);
std::string detectAndGetCoords(Mat image);

void processJsonParam(stringstream& stream);


CascadeClassifier gFaceCascade;


int main() {
    HttpServer server;
    server.config.port = 8080;

    server.default_resource["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        try {
            auto web_root_path = boost::filesystem::canonical("web");
            auto path = boost::filesystem::canonical(web_root_path / request->path);

            if (std::distance(web_root_path.begin(), web_root_path.end()) > std::distance(path.begin(), path.end()) ||
                !std::equal(web_root_path.begin(), web_root_path.end(), path.begin()))
            {
                throw std::invalid_argument("path must be within root path");
            }

            if (boost::filesystem::is_directory(path)) {
                path /= "index.html";
            }

            SimpleWeb::CaseInsensitiveMultimap header;

            auto ifs = std::make_shared<ifstream>();
            ifs->open(path.string(), ifstream::in | ios::binary | ios::ate);

            if (!*ifs) {
                throw std::invalid_argument("could not read file");
            }

            auto length = ifs->tellg();
            ifs->seekg(0, ios::beg);

            header.emplace("Content-Length", to_string(length));
            response->write(header);

            readAndSendData(response, ifs);
        }
        catch (const std::exception& e) {
            response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not open path " + request->path + ": " + e.what());
        }
    };

    server.resource["^/getResult$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        stringstream stream;
        stream << "<h1>Face Detection Service</h1>";

        auto getParams = request->parse_query_string();
        if (getParams.size() != 1) {
            stream << "<p>Wrong parameter</p>" << "<p>Usage: ?type={ json | image }</p>";
            response->write(stream);
            return;
        }

        if (gFaceCascade.empty()) {
            if (!gFaceCascade.load("haarcascade_frontalface_default.xml")) {
                stream << "<p>Error loading face cascade</p>";
                response->write(stream);
                return;
            }
        }

        auto value = (*getParams.find("type")).second; // get value by iterator
        if (value == "json") {
            try {
                processJsonParam(stream);
            }
            catch (std::exception e) {
                stream << e.what();
            }
        }
        else if (value == "image") {
            try {
                processImageParam(stream);
            }
            catch (std::exception e) {
                stream << e.what();
            }
        }
        else {
            stream << "<p>Wrong value: " << value << "</p>";
        }

        response->write(stream);
    };

    thread server_thread([&server]() {
        server.start();
    });

    // Wait for server to start so that the client can connect
    std::this_thread::sleep_for(std::chrono::seconds(1));

    cout << "Server started. localhost:8080" << endl;
    server_thread.join();

    return 0;
}


void readAndSendData(const shared_ptr<HttpServer::Response>& response, const shared_ptr<ifstream>& ifs) {
    // Read and send 128 KB at a time
    static vector<char> buffer(131072); // Safe when server is running on one thread
    streamsize read_length;
    if ((read_length = ifs->read(&buffer[0], static_cast<streamsize>(buffer.size())).gcount()) > 0) {
        response->write(&buffer[0], read_length);
        if (read_length == static_cast<streamsize>(buffer.size())) {
            response->send([response, ifs](const SimpleWeb::error_code& ec) {
                if (!ec) {
                    readAndSendData(response, ifs);
                }
                else {
                    cerr << "Connection interrupted" << endl;
                }
                });
        }
    }
}


void detectAndSave(Mat image) {
    Mat imageGray;
    cvtColor(image, imageGray, COLOR_BGR2GRAY);
    equalizeHist(imageGray, imageGray);
    
    //-- Detect faces
    std::vector<Rect> faces;
    gFaceCascade.detectMultiScale(imageGray, faces);

    for (size_t i = 0; i < faces.size(); i++) {
        Rect faceRect(faces[i].x, faces[i].width, faces[i].y, faces[i].height);
        rectangle(image, faceRect, Scalar(255, 0, 255));
    }

    if (!imwrite("web\\image.jpg", image)) {
        throw std::exception("Cannot save image");
    }
}


std::string detectAndGetCoords(Mat image) {
    Mat imageGray;
    cvtColor(image, imageGray, COLOR_BGR2GRAY);
    equalizeHist(imageGray, imageGray);

    //-- Detect faces
    std::vector<Rect> faces;
    gFaceCascade.detectMultiScale(imageGray, faces);

    ptree pt;
    ptree facesJson;
    for (size_t i = 0; i < faces.size(); i++) {
        ptree jsonFace;
        jsonFace.put("x", faces[i].x);
        jsonFace.put("y", faces[i].y);
        jsonFace.put("width", faces[i].width);
        jsonFace.put("height", faces[i].height);
        facesJson.push_back(std::make_pair("", jsonFace));
    }
    pt.add_child("faces", facesJson);

    std::ostringstream oss;
    json_parser::write_json(oss, pt);

    return oss.str();
}


void processImageParam(stringstream& stream) {
    auto capture = VideoCapture(0);
    if (!capture.isOpened()) {
        throw std::exception("Cannot open camera");
    }

    Mat image;
    if (!capture.read(image)) {
        throw std::exception("Cannot read image from camera");
    }

    try {
        detectAndSave(image);
    }
    catch (std::exception e) {
        stream << e.what();
        return;
    }

    stream << "<img src=\"image.jpg\">";
    std::this_thread::sleep_for(std::chrono::seconds(1));
}


void processJsonParam(stringstream& stream) {
    auto capture = VideoCapture(0);
    if (!capture.isOpened()) {
        throw std::exception("Cannot open camera");
    }

    Mat image;
    if (!capture.read(image)) {
        throw std::exception("Cannot read image from camera");
    }

    std::string jsonFaces;
    try {
        jsonFaces = detectAndGetCoords(image);
    }
    catch (std::exception e) {
        stream << e.what();
        return;
    }

    stream << jsonFaces;
}
