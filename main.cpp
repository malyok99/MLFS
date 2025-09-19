#include <iostream>
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
    
    std::cout << std::string(tab, ' ') << "└─ (" << buffer << ") / " << name << std::endl;
  }
  void serialize(std::ostream &os) override {};
  void deserialize(std::istream &is) override {};
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
  void serialize(std::ostream &os) override {};
  void deserialize(std::istream &is) override {};
  void addChild(std::unique_ptr<IFileObject> child){
    children.push_back(std::move(child));
  }
};

void addFile(const std::string &name){

}
void addDirectory(const std::string &name){

}

int main(){
  // make directory
  Directory homeDir;
  homeDir.name = "home";

  // add child (file)
  auto file1 = std::make_unique<File>();
  file1->name = "test.txt";
  homeDir.addChild(std::move(file1));

  auto dir1 = std::make_unique<Directory>();
  dir1->name = "another_dir";

  auto file2 = std::make_unique<File>();
  file2->name = "test2.txt";
  dir1->addChild(std::move(file2));

  homeDir.addChild(std::move(dir1));

  // display info
  homeDir.displayInfo();

  return 0;
}
