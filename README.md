# ClipboardTxtApp

ClipboardTxtApp là ứng dụng quản lý clipboard nhỏ gọn dành cho Windows. App chạy dưới system tray, lưu lịch sử text, file và hình ảnh, đồng thời hỗ trợ chọn nhanh nội dung clipboard để dán vào ô nhập liệu hoặc hộp thoại chọn file.

## Tính năng

- Lưu lịch sử clipboard gồm text, file và hình ảnh.
- Tìm kiếm nhanh trong nội dung, tiêu đề và đường dẫn file đã lưu.
- Xem trước text, file và hình ảnh trước khi chọn.
- Mở thư viện clipboard bằng `Ctrl+Alt+V`.
- Tự focus vào ô tìm kiếm khi cửa sổ clipboard xuất hiện.
- Dùng `↑` và `↓` để di chuyển giữa các mục.
- Nhấn `Enter` để chọn mục hiện tại.
- Nhấn `Esc` để đóng cửa sổ mà không chọn.
- Dán text thô hoặc chuyển text thành file `.txt`.
- Tự đề xuất chuyển văn bản dài thành file TXT khi dán vào ứng dụng hỗ trợ file.
- Tự phát hiện hộp thoại chọn file trong các ứng dụng được cho phép.
- Giới hạn số mục clipboard được lưu.
- Giới hạn dung lượng tối đa của mỗi file hoặc hình ảnh cache.
- Tự xóa cache cũ theo số ngày cấu hình; đặt `0` để không tự xóa.
- Xóa toàn bộ file và hình ảnh cache thủ công từ cửa sổ Cài đặt.

## Cách sử dụng

1. Tải và chạy `ClipboardTxtApp.exe` ở thư mục gốc của repository.
2. App sẽ chạy dưới system tray.
3. Copy text, file hoặc hình ảnh như bình thường để app lưu vào thư viện.
4. Nhấn `Ctrl+Alt+V` để mở cửa sổ chọn clipboard.
5. Nhập từ khóa tìm kiếm, dùng `↑`/`↓` để chọn và nhấn `Enter`.

Khi chọn một mục text bằng `Ctrl+Alt+V`, tùy chọn gửi dạng TXT mặc định được tắt để có thể dán text thô trực tiếp vào ô nhập liệu. Bạn vẫn có thể bật lại checkbox nếu muốn gửi dưới dạng file.

## Phím tắt

| Phím | Chức năng |
| --- | --- |
| `Ctrl+Alt+V` | Mở thư viện clipboard và focus ô tìm kiếm |
| `↑` / `↓` | Di chuyển mục đang chọn |
| `Enter` | Chọn mục hiện tại |
| `Esc` | Đóng cửa sổ clipboard |

## Cài đặt

Nhấn chuột trái hoặc chuột phải vào biểu tượng system tray, sau đó chọn **Cài đặt**.

| Cài đặt | Mặc định | Mô tả |
| --- | ---: | --- |
| Ngưỡng ký tự | `1800` | Số ký tự để app coi là văn bản dài |
| Số slot clipboard | `30` | Số mục tối đa được giữ trong thư viện |
| File cache tối đa | `100 MB` | Dung lượng tối đa cho mỗi file hoặc hình ảnh mới |
| Số ngày giữ cache | `7` | File cũ hơn số ngày này bị xóa khi app khởi động; `0` là không tự xóa |
| Luôn hỏi trước khi chuyển TXT | Bật | Hiện hộp xác nhận trước khi chuyển text dài thành file |
| Tự phát hiện hộp Choose file | Bật | Tự hỗ trợ hộp thoại chọn file trong ứng dụng được phép |
| Ứng dụng được phép | Danh sách trình duyệt và ứng dụng phổ biến | Mỗi process nhập trên một dòng |

## Dữ liệu ứng dụng

Dữ liệu được lưu cục bộ tại:

```text
%APPDATA%\ClipboardTxtApp
```

Các file chính:

```text
config.ini             Cấu hình ứng dụng
library.tsv            Lịch sử clipboard
generated\             File TXT, file và hình ảnh cache
```

ClipboardTxtApp không cần server và không tự gửi dữ liệu clipboard lên mạng.

## Build từ mã nguồn

### Yêu cầu

- Windows
- Trình biên dịch C++ hỗ trợ C++11
- MinGW `g++` trong biến môi trường `PATH`, hoặc CMake 3.20 trở lên

### Build nhanh bằng MinGW

```bat
build.bat
```

File sau khi build:

```text
build\ClipboardTxtApp.exe
```

Nếu app đang chạy, hãy thoát app trước khi build để Windows không khóa file `.exe`.

### Build bằng CMake

```powershell
cmake -S . -B build-cmake
cmake --build build-cmake --config Release
```

## Cấu trúc mã nguồn

```text
src\
  main.cpp               Khởi động app, system tray và hotkey
  AppState.*             Cấu hình và tiện ích filesystem
  ClipboardMonitor.*     Theo dõi, đọc và ghi clipboard
  ClipboardLibrary.*     Quản lý lịch sử clipboard
  FileDialogAssist.*     Hỗ trợ hộp thoại chọn file và ô nhập liệu
  TextFileConverter.*    Tạo file TXT từ clipboard
  AllowedApps.*          Kiểm tra danh sách ứng dụng được phép
  Ui.*                   Giao diện cài đặt và cửa sổ chọn clipboard
```

## License

Dự án được phát hành theo giấy phép [MIT](LICENSE).
