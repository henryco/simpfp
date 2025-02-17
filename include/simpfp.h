//
// Created by xd on 02/02/25.
//
// ReSharper disable CppDFANullDereference

#ifndef SIMPFP_H
#define SIMPFP_H

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <random>
#include <string.h>

namespace simpfp {

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
    void OpenFileDialog(const char *title, const char *default_path = nullptr, const char **filters = nullptr,
                        const Labels *labels = nullptr, bool read_only = false, bool accept_empty = false, bool dir_only = false);

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

    namespace internal_ {
        namespace fs = std::filesystem;

#define SORT_NONE 0

#define SORT_NAME 1
#define SORT_SIZE 2
#define SORT_TYPE 3
#define SORT_TIME 4

#define SORT_ASC 1
#define SORT_DSC 2

        inline const char *title        = nullptr;
        inline const char *file_path    = nullptr;
        inline bool       *open_ptr     = nullptr;
        inline bool        reset        = false;
        inline bool        closed       = true;
        inline bool        resizable    = true;
        inline bool        single       = true;
        inline bool        accept_empty = false;
        inline bool        dir_only     = false;
        inline bool        read_only    = false;

        inline const char **filters = nullptr;
        inline Labels       labels;

        inline void reset_vars() {
            internal_::open_ptr     = nullptr;
            internal_::filters      = nullptr;
            internal_::title        = nullptr;
            internal_::reset        = false;
            internal_::closed       = true;
            internal_::resizable    = true;
            internal_::single       = true;
            internal_::accept_empty = false;
            internal_::dir_only     = false;
            internal_::read_only    = false;
            internal_::labels       = Labels();
        }

        inline std::string glob_to_regex(const char *glob) {
            std::string regex_pattern = glob;
            for (size_t pos = 0; (pos = regex_pattern.find('.', pos)) != std::string::npos; ++pos) {
                regex_pattern.replace(pos, 1, "\\.");
                pos++;
            }
            for (size_t pos = 0; (pos = regex_pattern.find('*', pos)) != std::string::npos; ++pos) {
                regex_pattern.replace(pos, 1, ".*");
                pos += 1;
            }
            return regex_pattern;
        }

        inline fs::path generate_temp_file(const fs::path& directory) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint64_t> dist(100000, 999999);
            const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
            const auto filename = ".perm_check_" + std::to_string(timestamp) + "_" + std::to_string(dist(gen)) + ".tmp";
            return directory / filename;
        }

        inline bool can_write(const fs::path &path) {
            std::error_code ec;
            const auto      perms = std::filesystem::status(path, ec).permissions();

            if (ec)
                return false;

            if (fs::is_directory(path)) {
                fs::path temp_file = generate_temp_file(path);
                std::ofstream file(temp_file);
                if (!file.is_open())
                    return false;
                file.close();
                fs::remove(temp_file, ec);
                return true;
            }

            if ((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none ||
                  (perms & std::filesystem::perms::group_write) != std::filesystem::perms::none ||
                  (perms & std::filesystem::perms::others_write) != std::filesystem::perms::none) {
                return true;
            }

            std::ofstream file(path, std::ios::app);
            return file.is_open();
        }

        inline bool can_read(const fs::path &path) {
            std::error_code ec;
            const auto      perms = std::filesystem::status(path, ec).permissions();

            if (ec)
                return false;

            if ((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none ||
                (perms & std::filesystem::perms::group_read) != std::filesystem::perms::none ||
                (perms & std::filesystem::perms::others_read) != std::filesystem::perms::none) {
                const std::ifstream file(path);
                return file.good();
            }

            return false;
        }

        inline char *cpy_str(const char *str) {
            if (str == nullptr)
                return nullptr;
            const auto len  = std::strlen(str);
            auto      *buff = new char[len + 1];
            std::strcpy(buff, str);
            return buff;
        }

        inline void cat_str(char *dst, const char *src, const std::size_t dst_size) {
            const std::size_t free_len = dst_size - std::strlen(dst);
            const std::size_t src_len  = std::strlen(src);
            const std::size_t lim_len  = std::min(src_len, free_len);
            std::strncat(dst, src, lim_len);
        }

        inline fs::path load_path(const char *file) {
            if (file == nullptr)
                return fs::current_path();
            const auto path = fs::path(file);
            return fs::exists(path) ? path : fs::current_path();
        }

        inline char *format_time(const fs::file_time_type &ftime) {
            const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            const auto tt  = std::chrono::system_clock::to_time_t(sctp);
            const auto gmt = std::localtime(&tt);
            char       buffer[20];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", gmt);
            return cpy_str(buffer);
        }

        inline std::size_t get_unix_timestamp_ms(const fs::file_time_type &ftime) {
            const auto sctp = std::chrono::time_point_cast<std::chrono::milliseconds>(
                    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            return static_cast<std::size_t>(sctp.time_since_epoch().count());
        }

        struct FileInfo {
            const fs::path *path     = nullptr;
            const char     *name     = nullptr;
            const char     *type     = nullptr;
            const char     *time     = nullptr;
            std::size_t     size     = 0;
            std::size_t     date     = 0;
            int             index    = 0;
            bool            read     = false;
            bool            write    = false;
            bool            selected = false;

            FileInfo() = default;

            FileInfo(const FileInfo &other) {
                path     = other.path != nullptr ? (new fs::path(*other.path)) : nullptr;
                name     = cpy_str(other.name);
                type     = cpy_str(other.type);
                time     = cpy_str(other.time);
                read     = other.read;
                write    = other.write;
                selected = other.selected;
                size     = other.size;
                date     = other.date;
            }

            FileInfo(FileInfo &&other) noexcept {
                path     = other.path;
                name     = other.name;
                type     = other.type;
                time     = other.time;
                size     = other.size;
                date     = other.date;
                read     = other.read;
                write    = other.write;
                selected = other.selected;

                other.path = nullptr;
                other.name = nullptr;
                other.type = nullptr;
                other.time = nullptr;
            }

            FileInfo &operator=(FileInfo &&other) noexcept {
                if (&other == this)
                    return *this;

                if (name != nullptr && other.name != name)
                    delete[] name;
                if (type != nullptr && other.type != type)
                    delete[] type;
                if (time != nullptr && other.time != time)
                    delete[] time;
                if (path != nullptr && other.path != path)
                    delete path;

                path     = other.path;
                name     = other.name;
                type     = other.type;
                time     = other.time;
                size     = other.size;
                date     = other.date;
                read     = other.read;
                write    = other.write;
                selected = other.selected;

                other.path = nullptr;
                other.name = nullptr;
                other.type = nullptr;
                other.time = nullptr;

                return *this;
            }

            FileInfo &operator=(const FileInfo &other) {
                if (&other == this)
                    return *this;

                if (name != nullptr && other.name != name)
                    delete[] name;
                if (type != nullptr && other.type != type)
                    delete[] type;
                if (time != nullptr && other.time != time)
                    delete[] time;
                if (path != nullptr && other.path != path)
                    delete path;

                path     = other.path != nullptr ? (new fs::path(*other.path)) : nullptr;
                name     = cpy_str(other.name);
                type     = cpy_str(other.type);
                time     = cpy_str(other.time);
                selected = other.selected;
                read     = other.read;
                write    = other.write;
                size     = other.size;
                date     = other.date;

                return *this;
            }

            ~FileInfo() {
                if (name != nullptr)
                    delete[] name;
                if (type != nullptr)
                    delete[] type;
                if (time != nullptr)
                    delete[] time;
                if (path != nullptr)
                    delete path;
            }
        };

        struct FileContext {
            static constexpr std::size_t buffer_size = 256;
            char                        *buffer      = new char[buffer_size]{};

            const fs::path *path     = nullptr;
            FileInfo       *dirs     = nullptr;
            FileInfo       *files    = nullptr;
            FileInfo      **selected = nullptr;

            int selected_num = 0;
            int files_num    = 0;
            int dirs_num     = 0;
            int filter_idx   = 0;

            char sort_by   = SORT_NONE;
            char sort_type = SORT_NONE;

            bool accepted = false;
            bool peeked   = true;

            bool read  = false;
            bool write = false;

            ~FileContext() {
                if (selected != nullptr)
                    delete[] selected;
                if (files != nullptr)
                    delete[] files;
                if (dirs != nullptr)
                    delete[] dirs;
                if (path != nullptr)
                    delete path;
                if (buffer != nullptr)
                    delete[] buffer;
            }

            static void permissions(const fs::file_status &status, FileInfo &file) {
                const auto perms = status.permissions();
                file.read        = (perms & fs::perms::owner_read) != fs::perms::none ||
                            (perms & fs::perms::group_read) != fs::perms::none ||
                            (perms & fs::perms::others_read) != fs::perms::none;

                file.write = (perms & fs::perms::owner_write) != fs::perms::none ||
                             (perms & fs::perms::group_write) != fs::perms::none ||
                             (perms & fs::perms::others_write) != fs::perms::none;
            }

            static void load(FileContext **context, const char *path) {
                const auto og_f = load_path(path);
                const auto file = new fs::path(fs::is_directory(og_f) ? og_f : og_f.parent_path());

                char sort_by   = SORT_NONE;
                char sort_type = SORT_NONE;

                int filter = 0;

                if (*context != nullptr) {
                    filter    = (*context)->filter_idx;
                    sort_by   = (*context)->sort_by;
                    sort_type = (*context)->sort_type;
                    delete *context;
                }

                const auto new_context = new FileContext;
                new_context->path      = file;
                new_context->sort_by   = sort_by;
                new_context->sort_type = sort_type;
                new_context->read      = can_read(*file);
                new_context->write     = can_write(*file);

                if (fs::is_regular_file(og_f) && og_f.has_filename()) {
                    std::memset(new_context->buffer, 0, FileContext::buffer_size);
                    std::strncpy(new_context->buffer, og_f.filename().c_str(), FileContext::buffer_size - 1);
                }

                const int     init_dirs  = file->has_parent_path() ? 1 : 0;
                constexpr int init_files = 0;

                int dirs_num  = init_dirs;
                int files_num = init_files;

                std::error_code ec;
                for (fs::directory_iterator it(*file, ec); it != fs::directory_iterator(); it.increment(ec)) {
                    if (ec)
                        continue;

                    const auto &status = it->status(ec);
                    if (ec)
                        continue;

                    if (fs::is_directory(status))
                        dirs_num++;
                    else if (fs::is_regular_file(status))
                        files_num++;
                }

                const auto files = new FileInfo[files_num];
                const auto dirs  = new FileInfo[dirs_num];

                if (init_dirs > 0) {
                    dirs[0].path     = new fs::path(file->parent_path());
                    dirs[0].read     = can_read(file->parent_path());
                    dirs[0].selected = false;
                    dirs[0].index    = 0;
                    dirs[0].size     = 0;
                    dirs[0].date     = 0;
                    dirs[0].name     = new char[]{ ".." };
                    dirs[0].type     = new char[]{ ".." };
                    dirs[0].time     = new char[]{ ".." };
                }

                int d_ = init_dirs, f_ = init_files;

                std::regex pattern;
                bool       should_filter =
                        (filter >= 0) && (internal_::filters != nullptr) && (internal_::filters[filter] != nullptr);
                if (should_filter) {
                    const auto curr = internal_::filters[filter];
                    if (std::strlen(curr) == 0 || std::strcmp(curr, "*") == 0 || std::strcmp(curr, " ") == 0) {
                        should_filter = false;
                    }
                    else {
                        const auto regex_str = glob_to_regex(internal_::filters[filter]);
                        pattern              = std::regex(regex_str);
                    }
                }

                for (fs::directory_iterator it(*file, ec); it != fs::directory_iterator(); it.increment(ec)) {
                    if (ec)
                        continue;

                    const auto &status = it->status(ec);
                    if (ec)
                        continue;

                    if (fs::is_directory(status)) {
                        const auto ep = new fs::path(it->path());
                        permissions(status, dirs[d_]);
                        dirs[d_].path     = ep;
                        dirs[d_].name     = cpy_str(ep->filename().c_str());
                        dirs[d_].type     = new char[]{ "" };
                        dirs[d_].time     = new char[]{ "" };
                        dirs[d_].selected = false;
                        dirs[d_].size     = 0;
                        dirs[d_].date     = 0;
                        dirs[d_].index    = d_;
                        d_++;
                    }
                    else if (fs::is_regular_file(status)) {
                        const auto ep = new fs::path(it->path());
                        if (!should_filter || std::regex_match(ep->filename().string(), pattern)) {
                            const auto time_entry = fs::last_write_time(*ep);
                            permissions(status, files[f_]);
                            files[f_].path = ep;
                            files[f_].type = cpy_str(ep->has_extension() ? ep->extension().c_str() : new char[]{ "" });
                            files[f_].name = cpy_str(ep->filename().c_str());
                            files[f_].date = get_unix_timestamp_ms(time_entry);
                            files[f_].time = format_time(time_entry);
                            files[f_].size = it->file_size(ec);
                            files[f_].selected = false;
                            files[f_].index    = f_;
                            f_++;
                        }
                    }
                }

                new_context->files_num  = f_;
                new_context->files      = files;
                new_context->dirs_num   = dirs_num;
                new_context->dirs       = dirs;
                new_context->filter_idx = filter;

                *context = new_context;

                if (sort_by != SORT_NONE && sort_type != SORT_NONE)
                    FileContext::sort(*context, sort_by, sort_type);
            }

            static void reload(FileContext **context) {
                FileContext::load(context, (*context)->path->c_str());
            }

            static void select_single(FileContext *context, const int index) {
                if (context->selected_num == 1 && context->files[index].selected)
                    return;

                for (int c = 0; c < context->files_num; c++)
                    context->files[c].selected = false;
                context->files[index].selected = true;

                if (context->selected != nullptr && context->selected_num == 1) {
                    context->selected[0] = &context->files[index];
                    return;
                }

                if (context->selected != nullptr)
                    delete[] context->selected;
                context->selected     = new FileInfo *[1]{ &context->files[index] };
                context->selected_num = 1;
            }

            static void unselect_all(FileContext *context) {
                if (context->selected_num > 1)
                    std::memset(context->buffer, 0, FileContext::buffer_size);
                for (int i = 0; i < context->files_num; i++)
                    context->files[i].selected = false;
                if (context->selected != nullptr)
                    delete[] context->selected;
                context->selected_num = 0;
                context->selected     = nullptr;
            }

            static void select_add(FileContext *context, const int index) {
                if (context->selected == nullptr || context->selected_num <= 0) {
                    if (context->selected != nullptr)
                        delete[] context->selected;
                    context->selected              = new FileInfo *[1]{ &context->files[index] };
                    context->selected_num          = 1;
                    context->files[index].selected = true;
                    return;
                }

                if (context->files[index].selected)
                    return;

                const auto new_arr = new FileInfo *[context->selected_num + 1];
                std::memcpy(new_arr, context->selected, sizeof(FileInfo *) * context->selected_num);
                new_arr[context->selected_num] = &context->files[index];
                context->files[index].selected = true;

                delete[] context->selected;

                context->selected_num = context->selected_num + 1;
                context->selected     = new_arr;
            }

            static void select_range(FileContext *context, const int index) {
                if (context->selected == nullptr || context->selected_num <= 0) {
                    if (context->selected != nullptr)
                        delete[] context->selected;
                    context->selected              = new FileInfo *[1]{ &context->files[index] };
                    context->selected_num          = 1;
                    context->files[index].selected = true;
                    return;
                }

                const int idx_min = context->selected[0]->index;
                const int idx_max = context->selected[context->selected_num - 1]->index;

                const int idx_st = std::min(idx_min, index);
                const int idx_ed = std::max(idx_max, index);
                const int size   = idx_ed - idx_st + 1;

                if (size == 1) {
                    select_single(context, index);
                    return;
                }

                if (context->selected_num != size)
                    delete[] context->selected;

                context->selected     = new FileInfo *[size];
                context->selected_num = size;

                for (int c = 0, k = 0; c < context->files_num; c++) {
                    if (c >= idx_st && c <= idx_ed) {
                        context->files[c].selected = true;
                        context->selected[k++]     = &context->files[c];
                        continue;
                    }
                    context->files[c].selected = false;
                }
            }

            static void sort(FileContext *context, const char by, const char order) {
                context->sort_by   = by;
                context->sort_type = order;

                if (by <= 0 || order <= 0)
                    return;

                if (by == SORT_NAME) {
                    if (order == SORT_ASC) {
                        std::sort(context->files, context->files + context->files_num,
                                  [](const FileInfo &a, const FileInfo &b) { return std::strcmp(a.name, b.name) < 0; });
                    }
                    else {
                        std::sort(context->files, context->files + context->files_num,
                                  [](const FileInfo &a, const FileInfo &b) { return std::strcmp(a.name, b.name) > 0; });
                    }
                }

                else if (by == SORT_TYPE) {
                    if (order == SORT_ASC) {
                        std::sort(context->files, context->files + context->files_num,
                                  [](const FileInfo &a, const FileInfo &b) { return std::strcmp(a.type, b.type) < 0; });
                    }
                    else {
                        std::sort(context->files, context->files + context->files_num,
                                  [](const FileInfo &a, const FileInfo &b) { return std::strcmp(a.type, b.type) > 0; });
                    }
                }

                else if (by == SORT_SIZE) {
                    if (order == SORT_ASC) {
                        std::sort(context->files, context->files + context->files_num,
                                  [](const FileInfo &a, const FileInfo &b) { return a.size < b.size; });
                    }
                    else {
                        std::sort(context->files, context->files + context->files_num,
                                  [](const FileInfo &a, const FileInfo &b) { return a.size > b.size; });
                    }
                }

                else if (by == SORT_TIME) {
                    if (order == SORT_ASC) {
                        std::sort(context->files, context->files + context->files_num,
                                  [](const FileInfo &a, const FileInfo &b) { return a.date < b.date; });
                    }
                    else {
                        std::sort(context->files, context->files + context->files_num,
                                  [](const FileInfo &a, const FileInfo &b) { return a.date > b.date; });
                    }
                }
            }
        };

        inline bool display(const bool should) {
            if (!should)
              return false;

            const auto center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

            const bool show = ImGui::BeginPopupModal(internal_::title, internal_::open_ptr,
                                                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
                                                             (internal_::resizable ? 0 : ImGuiWindowFlags_NoResize));

            if (!show) {
                reset_vars();
            }

            return show;
        }

        inline bool show_dialog(const char *title, bool *open = nullptr, const ImVec2 *size = nullptr,
                                const bool resizable = true) {
            if (open != nullptr && !*open) {
                internal_::reset_vars();
                return display(false);
            }

            if (title == nullptr) {
                internal_::reset_vars();
                return display(false);
            }

            if (internal_::title == title) {
                if (internal_::closed) {
                    if (open != nullptr)
                        *open = false;
                    return display(false);
                }

                if (internal_::reset) {
                    if (size != nullptr) {
                        ImGui::SetNextWindowSizeConstraints(*size, ImVec2(-1, -1));
                        ImGui::SetNextWindowSize(*size, ImGuiCond_Appearing);
                    }
                    internal_::resizable = resizable;
                    internal_::reset     = false;
                    ImGui::OpenPopup(internal_::title, ImGuiPopupFlags_NoReopen);
                }

                internal_::open_ptr = open;
                return display(true);
            }

            internal_::open_ptr  = open;
            internal_::title     = title;
            internal_::reset     = false;
            internal_::closed    = false;
            internal_::resizable = resizable;

            if (size != nullptr) {
                ImGui::SetNextWindowSizeConstraints(*size, ImVec2(-1, -1));
                ImGui::SetNextWindowSize(*size, ImGuiCond_Appearing);
            }
            ImGui::OpenPopup(internal_::title, ImGuiPopupFlags_NoReopen);
            return display(true);
        }

        inline bool full_width_input(char *buffer, const std::size_t buffer_size, const float alpha = 1.0f,
                                     const float padding_right = 0.f) {
            static bool updated  = false;
            static auto callback = [](ImGuiInputTextCallbackData *data) -> int {
                if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
                    updated = true;
                return 0;
            };

            const auto input_pos  = ImGui::GetCursorScreenPos();
            const auto input_size = ImVec2(ImGui::GetContentRegionAvail().x - padding_right, ImGui::GetFrameHeight());

            ImGui::SetNextItemWidth(input_size.x);
            ImGui::InputText("##bordered_input", buffer, buffer_size, ImGuiInputTextFlags_CallbackEdit, callback);

            ImGui::GetWindowDrawList()->AddRect(input_pos,
                                                ImVec2(input_pos.x + input_size.x, input_pos.y + input_size.y),
                                                IM_COL32(0, 0, 0, 90 * alpha));

            if (updated) {
                updated = false;
                return true;
            }

            return false;
        }

        inline void buttons_dir(bool *create) {
            const float offset  = ImGui::GetStyle().FramePadding.x + ImGui::GetStyle().ItemSpacing.x;
            const float ok_size = ImGui::CalcTextSize(internal_::labels.main_create).x + offset;

            if (create == nullptr) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                ImGui::BeginDisabled(true);
            }

            if (ImGui::Button(internal_::labels.main_create, ImVec2(ok_size, 0))) {
                if (create != nullptr)
                    *create = true;
            }

            if (create == nullptr) {
                ImGui::EndDisabled();
                ImGui::PopStyleVar();
            }
        }

        inline void buttons_action(bool *cancel, bool *accept) {
            const float offset  = ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x * 2;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;

            const float ok_size = ImGui::CalcTextSize(internal_::labels.main_accept).x + offset;
            const float no_size = ImGui::CalcTextSize(internal_::labels.main_cancel).x + offset;

            const float available_width = ImGui::GetContentRegionMax().x;

            ImGui::SetCursorPosX(available_width - (ok_size + no_size + spacing));

            if (cancel == nullptr) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button(internal_::labels.main_cancel, ImVec2(ok_size, 0))) {
                if (cancel != nullptr)
                    *cancel = true;
            }
            if (cancel == nullptr) {
                ImGui::EndDisabled();
                ImGui::PopStyleVar();
            }

            ImGui::SameLine();

            if (accept == nullptr) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button(internal_::labels.main_accept, ImVec2(no_size, 0))) {
                if (accept != nullptr)
                    *accept = true;
            }
            if (accept == nullptr) {
                ImGui::EndDisabled();
                ImGui::PopStyleVar();
            }
        }

        inline bool select_filter(FileContext *context, const float alpha = 1.f) {
            ImGui::SameLine();
            constexpr float field_size      = 100.f;
            const float     available_width = ImGui::GetContentRegionMax().x;
            const float     spacing         = ImGui::GetStyle().ItemSpacing.x;

            ImGui::SetCursorPosX(available_width - (field_size) + spacing);
            ImGui::PushItemWidth(field_size - spacing);

            const auto input_pos  = ImGui::GetCursorScreenPos();
            const auto input_size = ImVec2(field_size - spacing, ImGui::GetFrameHeight());

            const auto prev_index = context->filter_idx;

            if (ImGui::BeginCombo("##filters", internal_::filters[context->filter_idx],
                                  ImGuiComboFlags_HeightRegular)) {
                for (int n = 0; internal_::filters[n] != nullptr; n++) {
                    const bool is_selected = (context->filter_idx == n);
                    ImGui::PushID(n);
                    if (ImGui::Selectable(internal_::filters[n], is_selected))
                        context->filter_idx = n;
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::GetWindowDrawList()->AddRect(input_pos,
                                                ImVec2(input_pos.x + input_size.x, input_pos.y + input_size.y),
                                                IM_COL32(0, 0, 0, 90 * alpha));

            return context->filter_idx != prev_index;
        }

    } // namespace internal_

    inline internal_::FileContext *context = nullptr;

    inline void OpenFileDialog(const char *title, const char *default_path, const char **filters, const Labels *labels,
                               const bool read_only, const bool accept_empty, const bool dir_only) {
        internal_::open_ptr     = nullptr;
        internal_::closed       = false;
        internal_::single       = true;
        internal_::reset        = true;
        internal_::title        = title;
        internal_::filters      = filters;
        internal_::dir_only     = dir_only;
        internal_::read_only    = read_only;
        internal_::file_path    = default_path;
        internal_::accept_empty = accept_empty || dir_only;

        if (labels != nullptr) {
            internal_::labels = *labels;
        }
        if (context != nullptr) {
            delete context;
            context = nullptr;
        }
    }

    inline bool FileDialogOpen() {
        return !internal_::closed;
    }

    inline void CloseFileDialog() {
        *internal_::open_ptr = false;
        internal_::closed    = true;
        if (context != nullptr) {
            delete context;
            context = nullptr;
        }
    }

    inline bool ShowFileDialog(const char *label, bool *open) {
        return internal_::show_dialog(label, open);
    }

    inline bool ShowFileDialog(const char *label, bool *open, const ImVec2 &size, const bool resize) {
        return internal_::show_dialog(label, open, &size, resize);
    }

    inline void EndFileDialog() {
        if (internal_::closed) {
            if (internal_::open_ptr != nullptr)
                *internal_::open_ptr = false;
            if (context != nullptr) {
                delete context;
                context = nullptr;
            }
            ImGui::EndPopup();
            return;
        }

        if (internal_::open_ptr != nullptr && !*internal_::open_ptr) {
            internal_::closed = true;
            if (context != nullptr) {
                delete context;
                context = nullptr;
            }
            ImGui::EndPopup();
            return;
        }

        if (context == nullptr) {
            internal_::FileContext::load(&context, internal_::file_path);
            if (internal_::filters != nullptr) {
                context->filter_idx = 0;
            }
        }

        static const float reserve_y = (ImGui::GetFrameHeight() * 4) + ImGui::GetStyle().ItemSpacing.y;

        constexpr float ratio     = 0.25f;
        const float     spacing_x = ImGui::GetStyle().ItemSpacing.x;
        const float     free_x    = ImGui::GetContentRegionAvail().x - spacing_x;

        const float free_x_uno = free_x * ratio;
        const float free_x_des = free_x * (1.f - ratio);

        const bool key_shift = ImGui::IsKeyPressed(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_LeftShift);
        const bool key_ctrl  = ImGui::IsKeyPressed(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
            *internal_::open_ptr = false;
            internal_::closed    = true;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            if (context != nullptr) {
                delete context;
                context = nullptr;
            }
            return;
        }

        ImGui::Spacing();
        ImGui::Text("%s", context->path->c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize("*").x - ImGui::GetStyle().FramePadding.x -
                        ImGui::GetStyle().ItemSpacing.x);
        ImGui::TextDisabled("%s", (key_shift || key_ctrl) ? "*" : "");

        ImGui::BeginChild("##region_dirs", ImVec2(free_x_uno, -reserve_y), ImGuiChildFlags_Borders);

        for (int i = 0; i < context->dirs_num; i++) {
            auto &dir      = context->dirs[i];
            bool  selected = false;

            ImGui::PushID(i);
            if (!dir.read) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                ImGui::BeginDisabled(true);
            }
            ImGui::Selectable(dir.name, &selected);
            if (!dir.read) {
                ImGui::EndDisabled();
                ImGui::PopStyleVar();
            }
            ImGui::PopID();

            if (selected && dir.read) {
                dir.read = internal_::can_read(internal_::fs::absolute(*dir.path));
                if (!dir.read)
                    continue;
                internal_::FileContext::load(&context, internal_::fs::absolute(*dir.path).c_str());
                context->peeked = false;
            }
        }

        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##region_files", ImVec2(free_x_des, -reserve_y), ImGuiChildFlags_Borders);

        bool selection_change = false;
        bool double_click     = false;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10.0f, 3.0f));
        if (ImGui::BeginTable("##files_table", 4,
                              ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable |
                                      ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn(" File", ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide);
            ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide);
            ImGui::TableHeadersRow();

            if (const auto sp = ImGui::TableGetSortSpecs(); sp != nullptr && sp->SpecsDirty) {
                if (const auto cs = sp->Specs) {
                    internal_::FileContext::sort(context, (cs->ColumnIndex + 1), cs->SortDirection);
                    internal_::FileContext::unselect_all(context);
                }
                sp->SpecsDirty = false;
            }

            for (int i = 0; i < context->files_num; i++) {
                const auto &file     = context->files[i];
                bool        selected = file.selected;

                const bool disable_select =
                        internal_::dir_only || !file.read || (!file.write && !internal_::read_only);

                ImGui::PushID(i);
                if (disable_select) {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                    ImGui::BeginDisabled(true);
                }

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::SameLine(0, 5);
                ImGui::Selectable(file.name, &selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
                    ImGui::GetIO().MouseClickedCount[ImGuiMouseButton_Left] == 2) {
                    double_click = true;
                }

                ImGui::TableNextColumn();
                ImGui::Text("%lu", file.size);

                ImGui::TableNextColumn();
                ImGui::Text("%s", file.type);

                ImGui::TableNextColumn();
                ImGui::Text("%s", file.time);
                ImGui::SameLine(0, 5);
                ImGui::TextDisabled(" ");

                if (disable_select) {
                    ImGui::EndDisabled();
                    ImGui::PopStyleVar();
                }

                ImGui::PopID();

                if (internal_::dir_only)
                    continue;

                if (selected == file.selected)
                    continue;

                selection_change = true;

                if (!internal_::single && key_ctrl) {
                    internal_::FileContext::select_add(context, i);
                    continue;
                }

                if (!internal_::single && key_shift) {
                    internal_::FileContext::select_range(context, i);
                    continue;
                }

                internal_::FileContext::select_single(context, i);
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();

        char                 *buffer  = context->buffer;
        constexpr std::size_t max_len = internal_::FileContext::buffer_size - 1;

        if (selection_change) {
            context->peeked = false;
            std::memset(buffer, 0, internal_::FileContext::buffer_size);

            if (context->selected != nullptr && context->selected_num == 1) {
                const auto str_src = context->selected[0]->name;
                std::strncpy(buffer, str_src, std::min(max_len, std::strlen(str_src)));
            }

            else if (context->selected != nullptr && context->selected_num > 1) {
                for (int k = 0; k < context->selected_num; k++) {
                    if (k > 0)
                        internal_::cat_str(buffer, "; ", max_len);
                    const auto str_src = context->selected[k]->name;
                    internal_::cat_str(buffer, str_src, max_len);
                }
            }
        }

        float alpha = 1.0f;
        if (internal_::dir_only || context->selected_num > 1) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            ImGui::BeginDisabled(true);
            alpha = 0.5f;
        }

        const bool      has_filters  = internal_::filters != nullptr;
        constexpr float filters_size = 100.f;

        if (internal_::full_width_input(buffer, max_len, alpha, has_filters ? filters_size : 0.f)) {
            context->peeked = false;
        }

        if (internal_::dir_only || context->selected_num > 1) {
            ImGui::EndDisabled();
            ImGui::PopStyleVar();
        }

        if (has_filters && internal_::select_filter(context)) {
            std::memset(context->buffer, 0, internal_::FileContext::buffer_size);
            internal_::FileContext::reload(&context);
            context->peeked = false;
        }

        ImGui::Spacing();

        const bool can_save = context->read && (context->write || internal_::read_only) &&
                              (internal_::accept_empty || std::strlen(buffer) > 0);

        bool create = false;
        bool cancel = false;
        bool accept = false;

        if (!internal_::read_only) {
            if (context->read && context->write)
                internal_::buttons_dir(&create);
            else
              internal_::buttons_dir(nullptr);
        } else {
            ImGui::Dummy(ImVec2(1.0f, ImGui::GetFrameHeight()));
        }

        ImGui::Spacing();

        internal_::buttons_action(&cancel, can_save ? &accept : nullptr);

        if (cancel) {
            *internal_::open_ptr = false;
            internal_::closed    = true;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            if (context != nullptr) {
                delete context;
                context = nullptr;
            }
            return;
        }

        static char new_name_buffer[128];
        static bool allowed = false;
        if (!internal_::read_only && create) {
            allowed = false;
            std::memset(new_name_buffer, 0, sizeof(new_name_buffer));
            ImGui::OpenPopup(internal_::labels.dir_title);
        }

        const auto offset     = ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x * 2;
        const auto title_size = ImGui::CalcTextSize(internal_::labels.dir_input).x + offset * 4;

        const auto center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(title_size, 0));

        if (!internal_::read_only &&
            ImGui::BeginPopupModal(internal_::labels.dir_title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::Text("%s", internal_::labels.dir_input);
            ImGui::Spacing();

            if (internal_::full_width_input(new_name_buffer, sizeof(new_name_buffer))) {
                allowed = std::strlen(new_name_buffer) > 0 &&
                          !internal_::fs::exists(internal_::fs::absolute(*context->path / new_name_buffer));
            }

            ImGui::Spacing();
            ImGui::Spacing();

            const float ok_size = ImGui::CalcTextSize(internal_::labels.dir_accept).x + offset;
            const float no_size = ImGui::CalcTextSize(internal_::labels.dir_cancel).x + offset;
            const float free_w  = ImGui::GetContentRegionMax().x;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;

            ImGui::SetCursorPosX(free_w - (ok_size + no_size + spacing));

            if (!allowed) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button(internal_::labels.dir_accept, ImVec2(ok_size, 0)) && allowed) {
                allowed            = false;
                const auto new_dir = internal_::fs::absolute(*context->path / new_name_buffer);
                if (!internal_::fs::exists(new_dir) && internal_::fs::create_directory(new_dir)) {
                    std::memset(new_name_buffer, 0, sizeof(new_name_buffer));
                    ImGui::CloseCurrentPopup();
                    internal_::FileContext::reload(&context);
                    context->peeked = false;
                }
            }
            if (!allowed) {
                ImGui::EndDisabled();
                ImGui::PopStyleVar();
            }

            ImGui::SameLine();
            if (ImGui::Button(internal_::labels.dir_cancel, ImVec2(no_size, 0))) {
                std::memset(new_name_buffer, 0, sizeof(new_name_buffer));
                ImGui::CloseCurrentPopup();
                allowed = false;
            }
            ImGui::EndPopup();
        }

        if (accept || (can_save && double_click)) {
            context->accepted = true;
            ImGui::EndPopup();
            return;
        }

        ImGui::EndPopup();

        if (internal_::open_ptr != nullptr && !*internal_::open_ptr) {
            internal_::closed = true;
            if (context != nullptr) {
                delete context;
                context = nullptr;
            }
        }
    }

    inline void UnselectAll() {
        if (context == nullptr)
            return;
        if (context->selected != nullptr)
            delete[] context->selected;
        context->selected = nullptr;
        context->accepted = false;
        context->peeked   = true;
    }

    inline void ResetBuffer() {
        if (context == nullptr)
            return;
        std::memset(context->buffer, 0, internal_::FileContext::buffer_size);
        context->accepted = false;
        context->peeked   = true;
    }

    inline void Reload() {
        if (context == nullptr)
            return;
        std::memset(context->buffer, 0, internal_::FileContext::buffer_size);
        internal_::FileContext::reload(&context);
        context->accepted = false;
        context->peeked   = true;
    }

    inline long CountSelected() {
        if (context == nullptr || context->selected == nullptr)
            return 0;
        return context->selected_num;
    }

    inline const char *CurrentPath() {
        if (context == nullptr || context->path == nullptr)
            return nullptr;
        return context->path->c_str();
    }

    inline bool PeekSelected(char *buffer_out, const std::size_t size) {
        if (context == nullptr || context->peeked)
            return false;

        context->peeked = true;

        if (context->selected == nullptr || context->selected_num <= 0) {
            context->peeked = true;

            if (context->buffer == nullptr || std::strlen(context->buffer) <= 0) {
                std::memset(buffer_out, 0, size);
                std::strncpy(buffer_out, context->path->c_str(), size);
                return true;
            }

            std::memset(buffer_out, 0, size);
            std::strncpy(buffer_out, internal_::fs::absolute(*context->path / context->buffer).c_str(), size);
            return true;
        }

        const auto &selected = context->selected[0];
        if (selected == nullptr) {
            return false;
        }

        if (context->selected_num == 1) {
            std::memset(buffer_out, 0, size);
            std::strncpy(buffer_out, internal_::fs::absolute(selected->path->parent_path() / context->buffer).c_str(),
                         size);
            return true;
        }

        std::memset(buffer_out, 0, size);
        std::strncpy(buffer_out, internal_::fs::absolute(*selected->path).c_str(), size);
        return true;
    }

    inline bool PeekSelected(char *buffer_out, const std::size_t size, const std::size_t index) {
        static std::size_t last_index = 0;
        internal_::single             = false;

        if (index < last_index || context == nullptr || context->peeked) {
            if (context != nullptr)
                context->peeked = true;
            last_index = 0;
            return false;
        }

        last_index = index;
        if (context->selected == nullptr || context->selected_num <= 0 || context->selected_num <= index) {
            context->peeked = true;
            last_index      = 0;

            if (index == 0) {

                if (context->buffer == nullptr || std::strlen(context->buffer) <= 0) {
                    std::memset(buffer_out, 0, size);
                    std::strncpy(buffer_out, context->path->c_str(), size);
                    return true;
                }

                std::memset(buffer_out, 0, size);
                std::strncpy(buffer_out, internal_::fs::absolute(*context->path / context->buffer).c_str(), size);
                return true;
            }
            return false;
        }

        const auto &selected = context->selected[index];
        if (selected == nullptr) {
            context->peeked = true;
            last_index      = 0;
            return false;
        }

        if (index >= context->selected_num - 1) {
            context->peeked = true;
            last_index      = 0;
        }

        if (context->selected_num == 1) {
            context->peeked = true;
            last_index      = 0;
            std::memset(buffer_out, 0, size);
            std::strncpy(buffer_out, internal_::fs::absolute(selected->path->parent_path() / context->buffer).c_str(),
                         size);
            return true;
        }

        std::memset(buffer_out, 0, size);
        std::strncpy(buffer_out, internal_::fs::absolute(*selected->path).c_str(), size);
        return true;
    }

    inline bool FileAccepted(char *buffer_out, const std::size_t size, const std::size_t index) {
        static std::size_t last_index = 0;
        internal_::single             = false;

        if (index < last_index || context == nullptr || !context->accepted) {
            if (context != nullptr)
                context->accepted = false;
            last_index = 0;
            return false;
        }

        last_index = index;

        if (context->selected == nullptr || context->selected_num <= 0 || context->selected_num <= index) {
            context->accepted = false;
            last_index        = 0;

            if (index == 0) {

                if (context->buffer == nullptr || std::strlen(context->buffer) <= 0) {
                    if (internal_::accept_empty) {
                        std::memset(buffer_out, 0, size);
                        std::strncpy(buffer_out, context->path->c_str(), size);
                        return true;
                    }
                    return false;
                }

                std::memset(buffer_out, 0, size);
                std::strncpy(buffer_out, internal_::fs::absolute(*context->path / context->buffer).c_str(), size);
                return true;
            }
            return false;
        }

        const auto &selected = context->selected[index];
        if (selected == nullptr) {
            context->accepted = false;
            last_index        = 0;
            return false;
        }

        if (context->selected_num == 1) {
            context->accepted = false;
            last_index        = 0;

            std::memset(buffer_out, 0, size);
            std::strncpy(buffer_out, internal_::fs::absolute(selected->path->parent_path() / context->buffer).c_str(),
                         size);
            return true;
        }

        std::memset(buffer_out, 0, size);
        std::strncpy(buffer_out, internal_::fs::absolute(*selected->path).c_str(), size);

        if (index >= context->selected_num - 1) {
            context->accepted = false;
            last_index        = 0;
        }

        return true;
    }

    inline bool FileAccepted(char *buffer_out, const std::size_t size) {
        if (context == nullptr || !context->accepted)
            return false;

        context->accepted = false;

        if (context->selected == nullptr || context->selected_num <= 0) {
            if (context->buffer == nullptr || std::strlen(context->buffer) <= 0) {
                if (internal_::accept_empty) {
                    std::memset(buffer_out, 0, size);
                    std::strncpy(buffer_out, context->path->c_str(), size);
                    return true;
                }
                return false;
            }

            std::memset(buffer_out, 0, size);
            std::strncpy(buffer_out, internal_::fs::absolute(*context->path / context->buffer).c_str(), size);
            return true;
        }

        std::memset(buffer_out, 0, size);
        std::strncpy(buffer_out,
                     internal_::fs::absolute(context->selected[0]->path->parent_path() / context->buffer).c_str(),
                     size);
        return true;
    }

} // namespace simpfp

#endif // SIMPFP_H
