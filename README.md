# SimFp - Simple File-Picker for ImGUI

![Demo](https://github.com/henryco/simpfp/blob/master/media/1.png?raw=true)

- Header-only
- Cross-platform
- Lightweight and fast, no external dependencies (stl only)
- Requires C++17 and above
- Supports permissions
- Supports multi-files
- Supports glob filters

## Example
Very minimalistic example below:

```C++
#include <simpfp.h>

...

static constexpr auto min_size  = ImVec2(900, 500);
static const char    *filters[] = { "*", "*.cmake", "*.txt", "*.md", nullptr };
static bool           fp_open   = true;

if (!simpfp::FileDialogOpen())
    simpfp::OpenFileDialog("Select a file", nullptr, filters, nullptr, false, false, false);

if (simpfp::ShowFileDialog("Select a file", &fp_open, min_size, true)) {

    if (char file[256]; simpfp::PeekSelected(file, sizeof(file))) {
        std::cout << "peek: " << file << '\n';
    }

    if (char file[256]; simpfp::FileAccepted(file, sizeof(file))) {
        std::cout << "accepted: " << file << '\n';
        simpfp::CloseFileDialog();
    }

    simpfp::EndFileDialog();
}

...
```

## API
Api is extremely simple and (hopefully) self-describing

```C++
namespace  simpfp {

    struct Labels {
        const char *main_accept = "Select";
        const char *main_cancel = "Cancel";
        const char *main_create = "New Directory";
        const char *dir_title   = "New Folder";
        const char *dir_input   = "Enter a new folder name:";
        const char *dir_accept  = "OK";
        const char *dir_cancel  = "Cancel";
    };
    
    /**
     * @param title title of the file picker window
     * @param default_path can be path to a directory or a file
     * @param filters array of glob-style strings, ie: *.cpp / *.txt / *.md / etc.
     * <br/><b>Must be terminated with nullptr!</b>
     * @param labels custom labels
     * @param read_only require only read permission
     * @param accept_empty allow empty selection
     * @param dir_only show/accept only directories
     */
    void OpenFileDialog(const char   *title,
                        const char   *default_path = nullptr,
                        const char  **filters      = nullptr,
                        const Labels *labels       = nullptr,
                        bool          read_only    = false,
                        bool          accept_empty = false,
                        bool          dir_only     = false);
    
    bool ShowFileDialog(const char *label, bool *open, const ImVec2 &size, bool resize = true);
    bool ShowFileDialog(const char *label, bool *open = nullptr);
    
    bool FileAccepted(char *buffer_out, std::size_t size);
    bool FileAccepted(char *buffer_out, std::size_t size, std::size_t index);
    
    bool PeekSelected(char *buffer_out, std::size_t size);
    bool PeekSelected(char *buffer_out, std::size_t size, std::size_t index);
    
    const char *CurrentPath();
    
    bool FileDialogOpen();
    void CloseFileDialog();
    void EndFileDialog();
    
    long CountSelected();
    void UnselectAll();
    void ResetBuffer();
    void Reload();

}
```
