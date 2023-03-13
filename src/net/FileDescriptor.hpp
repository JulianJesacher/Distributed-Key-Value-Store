#include <optional>

namespace net {

class FileDescriptor {
public:

    FileDescriptor() = default;
    explicit FileDescriptor(int fd);
    ~FileDescriptor();

    //Not copyable
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    //Movable
    FileDescriptor(FileDescriptor&&) noexcept;
    FileDescriptor& operator=(FileDescriptor&&) noexcept;

    [[nodiscard]] int unwrap() const;

private:
    std::optional<int> fd_{ std::nullopt };
};

}