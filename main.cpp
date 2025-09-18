#include <iostream>
#include <vector>

// file object
class FileObjectSystem {
public:
  virtual void displayInfo() = 0;
  virtual void serialize(std::ostream) = 0;
  virtual void deserialize(std::istream) = 0;
};

// file
class File:FileObjectSystem {
public:
  std::string name;
  std::vector<char> data;
  void displayInfo() override {};
  void serialize(std::ostream) override {};
  void deserialize(std::istream) override {};
};

// directory
class Directory:FileObjectSystem {
public:
  std::string name;
  std::vector<File> data;
  void displayInfo() override {
    std::cout << "Directory name: " << name << std::endl;
    for(auto &f : data){
      std::cout << "File name: " << f.name << std::endl;
    }
  }
  void serialize(std::ostream) override {};
  void deserialize(std::istream) override {};
};

int main(){
  // make directory
  Directory homeDir;
  homeDir.name = "home";

  // add child (file)
  File myFile;
  myFile.name = "file.txt";
  homeDir.data.push_back(myFile);

  File myFile1;
  myFile1.name = "file1.txt";
  homeDir.data.push_back(myFile1);

  // display info
  homeDir.displayInfo();

  return 0;
}
