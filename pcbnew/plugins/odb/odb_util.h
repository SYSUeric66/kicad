
#include <iostream>
#include <filesystem>
namespace fs = std::filesystem;

class ODB_GENERATE_INTERFACE
{
    ODB_GENERATE_INTERFACE() {}
    virtual ~ODB_GENERATE_INTERFACE() {}
    virtual void GenerateFiles( ODB_WRITER& writer ) = 0;
};

class ODB_WRITER
{
public:
    ODB_WRITER(const std::string &filename);
    ~ODB_WRITER();

    void write_line(const std::string &var, const std::string &value);
    void write_line(const std::string &var, int value);
    template <typename T> void write_line_enum(const std::string &var, const T &value)
    {
        write_line(var, enum_to_string(value));
    }

    class ArrayProxy {
        friend ODB_WRITER;

    public:
        ~ArrayProxy();
    };

private:
    wxString currentProjectDirPath;
};


class StructuredTextWriter {
public:
    StructuredTextWriter(std::ostream &s);
    void write_line(const std::string &var, const std::string &value);
    void write_line(const std::string &var, int value);
    template <typename T> void write_line_enum(const std::string &var, const T &value)
    {
        write_line(var, enum_to_string(value));
    }

    class ArrayProxy {
        friend StructuredTextWriter;

    public:
        ~ArrayProxy();

    private:
        ArrayProxy(StructuredTextWriter &writer, const std::string &a);

        StructuredTextWriter &writer;

        ArrayProxy(ArrayProxy &&) = delete;
        ArrayProxy &operator=(ArrayProxy &&) = delete;

        ArrayProxy(ArrayProxy const &) = delete;
        ArrayProxy &operator=(ArrayProxy const &) = delete;
    };

    [[nodiscard]] ArrayProxy make_array_proxy(const std::string &a)
    {
        return ArrayProxy(*this, a);
    }

private:
    void write_indent();
    void begin_array(const std::string &a);
    void end_array();
    std::ostream &ost;
    bool in_array = false;
};



class TreeWriter {
    friend class TreeWriterPrefixed;

public:
    class FileProxy {
        friend TreeWriter;

    private:
        FileProxy(TreeWriter &writer, const fs::path &p);
        TreeWriter &writer;

    public:
        std::ostream &stream;

        ~FileProxy();

        FileProxy(FileProxy &&) = delete;
        FileProxy &operator=(FileProxy &&) = delete;

        FileProxy(FileProxy const &) = delete;
        FileProxy &operator=(FileProxy const &) = delete;
    };

    [[nodiscard]] FileProxy create_file(const fs::path &path)
    {
        return FileProxy(*this, path);
    }

private:
    virtual std::ostream &create_file_internal(const fs::path &path) = 0;
    virtual void close_file() = 0;
};

class TreeWriterPrefixed : public TreeWriter {
public:
    TreeWriterPrefixed(TreeWriter &parent, const fs::path &prefix);

private:
    std::ostream &create_file_internal(const fs::path &path) override;
    void close_file() override;

    TreeWriter &parent;
    const fs::path prefix;
};

