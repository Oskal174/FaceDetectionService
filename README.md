# Face detection service

## Requirements

* Boost 1.70.0 (for Simple-Web-Server)
* OpenCV 4.1.1

## Installation

VS project settings
1. Configuration Properties -> Debugging -> Environment: change path `D:\opencv` to yours OpenCV root path
2. C/C++ -> General -> Additional Include Directories: change path `D:\opencv` to yours OpenCV root path
3. Linker -> General -> Additional Library Directories: change path `D:\opencv` to yours OpenCV root path
4. C/C++ -> Additional Include Directories: change path `D:\boost_1_70_0` to yours Boost root path
5. Change settings "Linker -> Additional Library Directories: change path `D:\boost_1_70_0` to yours Boost root path (make sure your Boost was compiled)

## Usage

1. Compile solution (Debug/Release x64)
2. Run binary file from console
3. Open `localhost:8080/getResult?type={json|image}` in web browser

# Other

## Installing/Compiling Boost

1. Download .zip archive from official site (https://www.boost.org/users/download/)
2. Unzip it in any folder
3. Open Visual Studio 2019 Developer Command Prompt and go to Boost root folder
4. Run bootstrap.bat - it`s prepare Boost to build
5. Run b2.exe - it`s build Boost library