#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <ctime>
#include <cstring>
#include <unordered_map>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fuse3/fuse.h>
#include <unistd.h>

enum class NodeType : uint8_t { File = 1, Directory = 2 };

class IFileObject {
public:
  virtual ~IFileObject() = default;
  virtual NodeType type() const = 0;
  virtual void serialize(std::ostream &os) = 0;
  virtual void deserialize(std::istream &is) = 0;
  virtual size_t getSize() const = 0;
  virtual time_t getCTime() const = 0;
  virtual time_t getMTime() const = 0;
  virtual const std::string& getName() const = 0;
  virtual IFileObject* findChild(const std::string& name) = 0;
};

class File : public IFileObject {
public:
  std::string name;
  std::vector<char> data;
  time_t ctime;
  time_t mtime;

  File() : ctime(std::time(nullptr)), mtime(std::time(nullptr)) {}
  File(const std::string& n) : name(n), ctime(std::time(nullptr)), mtime(std::time(nullptr)) {}

  NodeType type() const override { return NodeType::File; }
  const std::string& getName() const override { return name; }

  void serialize(std::ostream &os) override {
    uint32_t nameLen = name.size();
    os.write(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
    os.write(name.data(), nameLen);

    uint32_t dataLen = data.size();
    os.write(reinterpret_cast<char*>(&dataLen), sizeof(dataLen));
    if (dataLen) os.write(data.data(), dataLen);
    
    os.write(reinterpret_cast<char*>(&ctime), sizeof(ctime));
    os.write(reinterpret_cast<char*>(&mtime), sizeof(mtime));
  }

  void deserialize(std::istream &is) override {
    uint32_t nameLen;
    is.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
    name.resize(nameLen);
    if (nameLen) is.read(name.data(), nameLen);

    uint32_t dataLen;
    is.read(reinterpret_cast<char*>(&dataLen), sizeof(dataLen));
    data.resize(dataLen);
    if (dataLen) is.read(data.data(), dataLen);
    
    is.read(reinterpret_cast<char*>(&ctime), sizeof(ctime));
    is.read(reinterpret_cast<char*>(&mtime), sizeof(mtime));
  }

  size_t getSize() const override { return data.size(); }
  time_t getCTime() const override { return ctime; }
  time_t getMTime() const override { return mtime; }
  
  IFileObject* findChild(const std::string& name) override { return nullptr; }
};

class Directory : public IFileObject {
public:
  std::string name;
  std::vector<std::unique_ptr<IFileObject>> children;
  time_t ctime;
  time_t mtime;

  Directory() : ctime(std::time(nullptr)), mtime(std::time(nullptr)) {}
  Directory(const std::string& n) : name(n), ctime(std::time(nullptr)), mtime(std::time(nullptr)) {}

  NodeType type() const override { return NodeType::Directory; }
  const std::string& getName() const override { return name; }

  void serialize(std::ostream &os) override {
    uint32_t nameLen = name.size();
    os.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
    os.write(name.data(), nameLen);

    uint32_t childCount = children.size();
    os.write(reinterpret_cast<const char*>(&childCount), sizeof(childCount));

    for (const auto &c : children) {
      uint8_t tag = static_cast<uint8_t>(c->type());
      os.write(reinterpret_cast<const char*>(&tag), sizeof(tag));
      c->serialize(os);
    }
    
    os.write(reinterpret_cast<char*>(&ctime), sizeof(ctime));
    os.write(reinterpret_cast<char*>(&mtime), sizeof(mtime));
  }

  void deserialize(std::istream &is) override {
    uint32_t len;
    is.read(reinterpret_cast<char*>(&len), sizeof(len));
    name.resize(len);
    if (len) is.read(name.data(), len);

    uint32_t count;
    is.read(reinterpret_cast<char*>(&count), sizeof(count));

    children.clear();
    children.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      uint8_t tag;
      is.read(reinterpret_cast<char*>(&tag), sizeof(tag));

      std::unique_ptr<IFileObject> child;
      if (tag == static_cast<uint8_t>(NodeType::File)) child = std::make_unique<File>();
      else child = std::make_unique<Directory>();

      child->deserialize(is);
      children.push_back(std::move(child));
    }
    
    is.read(reinterpret_cast<char*>(&ctime), sizeof(ctime));
    is.read(reinterpret_cast<char*>(&mtime), sizeof(mtime));
  }

  void addChild(std::unique_ptr<IFileObject> c){
    children.push_back(std::move(c));
    mtime = std::time(nullptr);
  }
  
  IFileObject* findChild(const std::string& childName) override {
    for (const auto& child : children) {
      if (child->getName() == childName) {
        return child.get();
      }
    }
    return nullptr;
  }
  
  size_t getSize() const override { return 4096; } // Directory size
  time_t getCTime() const override { return ctime; }
  time_t getMTime() const override { return mtime; }
};

// Global root directory
std::unique_ptr<Directory> root;

// Helper function to split a path into components
std::vector<std::string> splitPath(const char* path) {
  std::vector<std::string> components;
  if (path == nullptr || strlen(path) == 0) return components;
  
  char* path_copy = strdup(path);
  char* token = strtok(path_copy, "/");
  
  while (token != nullptr) {
    components.push_back(token);
    token = strtok(nullptr, "/");
  }
  
  free(path_copy);
  return components;
}

// Find a node by path
IFileObject* findNode(const char* path) {
  if (strcmp(path, "/") == 0) return root.get();
  
  std::vector<std::string> components = splitPath(path);
  IFileObject* current = root.get();
  
  for (const auto& comp : components) {
    if (current->type() != NodeType::Directory) return nullptr;
    current = static_cast<Directory*>(current)->findChild(comp);
    if (current == nullptr) return nullptr;
  }
  
  return current;
}

// FUSE operations
static int myfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
  (void) fi;
  memset(stbuf, 0, sizeof(struct stat));
  
  IFileObject* node = findNode(path);
  if (node == nullptr) return -ENOENT;
  
  if (node->type() == NodeType::Directory) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else {
    stbuf->st_mode = S_IFREG | 0644;
    stbuf->st_nlink = 1;
    stbuf->st_size = node->getSize();
  }
  
  stbuf->st_ctime = node->getCTime();
  stbuf->st_mtime = node->getMTime();
  stbuf->st_atime = time(nullptr);
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();
  
  return 0;
}

static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
  (void) offset;
  (void) fi;
  (void) flags;
  
  IFileObject* node = findNode(path);
  if (node == nullptr || node->type() != NodeType::Directory) 
    return -ENOENT;
  
  Directory* dir = static_cast<Directory*>(node);
  
  filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
  filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
  
  for (const auto& child : dir->children) {
    filler(buf, child->getName().c_str(), NULL, 0, FUSE_FILL_DIR_PLUS);
  }
  
  return 0;
}

static int myfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
  (void) fi;
  
  IFileObject* node = findNode(path);
  if (node == nullptr || node->type() != NodeType::File) 
    return -ENOENT;
  
  File* file = static_cast<File*>(node);
  size_t file_size = file->getSize();
  
  if (offset < 0) return -EINVAL;
  if (offset >= file_size) return 0;
  
  if (offset + size > file_size)
    size = file_size - offset;
  
  memcpy(buf, file->data.data() + offset, size);
  return size;
}

static struct fuse_operations myfs_oper;

int main(int argc, char *argv[]) {
  myfs_oper.getattr = myfs_getattr;
  myfs_oper.readdir = myfs_readdir;
  myfs_oper.read = myfs_read;

  // Initialize filesystem
  root = std::make_unique<Directory>();
  root->name = "";
  
  // Add example structure
  auto f1 = std::make_unique<File>("empty.txt");
  root->addChild(std::move(f1));

  auto dir1 = std::make_unique<Directory>("another_dir");
  
  auto f2 = std::make_unique<File>("secret.txt");
  std::string content = "top secret!";
  f2->data.assign(content.begin(), content.end());
  dir1->addChild(std::move(f2));
  
  root->addChild(std::move(dir1));

  // Start FUSE
  return fuse_main(argc, argv, &myfs_oper, NULL);
}
