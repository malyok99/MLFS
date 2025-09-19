#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <ctime>

// instructor
class IFileObject {
public:
  virtual void displayInfo(int tab = 0) = 0;
  virtual void serialize(std::ostream &os) = 0;
  virtual void deserialize(std::istream &is) = 0;
};

// file
class File : public IFileObject {
private:
  time_t currentTime = time(0);
  tm* date = localtime(&currentTime);
public:
  std::string name;
  std::vector<char> data;

  void displayInfo(int tab = 0) override {
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", date);
    
    std::cout << std::string(tab, ' ') << "\\_ (" << buffer << ") | " << name << std::endl;
  }
  void serialize(std::ostream &os) override {
    uint32_t nameLen = name.size();
    os.write(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
    os.write(name.data(), nameLen);

    uint32_t dataLen = data.size();
    os.write(reinterpret_cast<char*>(&dataLen), sizeof(dataLen));
    os.write(data.data(), dataLen);
  }
  void deserialize(std::istream &is) override {
    uint32_t nameLen = name.size();
    is.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
    name.resize(nameLen);
    is.read(name.data(), nameLen);

    uint32_t dataLen = data.size();
    is.read(reinterpret_cast<char*>(&dataLen), sizeof(dataLen));
    data.resize(dataLen);
    is.read(data.data(), dataLen);
  }
};

// directory
class Directory : public IFileObject {
public:
  std::string name;
  std::vector<std::unique_ptr<IFileObject>> children;

  void displayInfo(int tab = 0) override {
    std::cout << std::string(tab, ' ') << name << std::endl;
    for(auto &f : children){
      f->displayInfo(tab + 2);
    }
  }
  void serialize(std::ostream &os) override {
    uint32_t nameLen = name.size();
    os.write(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
    os.write(name.data(), nameLen);

    uint32_t dataLen = children.size();
    os.write(reinterpret_cast<const char*>(&dataLen), sizeof(dataLen));
    for (const auto& f : children) {
      f->serialize(os);
    }
  }
  void deserialize(std::istream &is) override {
    uint32_t nameLen;
    is.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
    name.resize(nameLen);
    is.read(name.data(), nameLen);

    uint32_t dataLen;
    is.read(reinterpret_cast<char*>(&dataLen), sizeof(dataLen));
    children.resize(dataLen);
    for (auto& f : children) {
      f->deserialize(is);
    }
  }
  void addChild(std::unique_ptr<IFileObject> child){
    children.push_back(std::move(child));
  }
};

auto addFile(const std::string &name){
  auto file = std::make_unique<File>();
  file->name = name;
  return file;
}

auto addDirectory(const std::string &name){
  auto dir = std::make_unique<Directory>();
  dir->name = "another_dir";
  return dir;
}

int main(){
  // make directory
  Directory homeDir;
  homeDir.name = "home";

  // add child (file)
  auto file1 = addFile("empty.txt");
  homeDir.addChild(std::move(file1));

  auto dir1 = addDirectory("dir1");

  auto file2 = addFile("secret.txt");
  dir1->addChild(std::move(file2));

  homeDir.addChild(std::move(dir1));

  // display info
  homeDir.displayInfo();

  return 0;
}
