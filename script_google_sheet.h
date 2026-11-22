// ==========================================
// 1. CẤU HÌNH CƠ BẢN (Biến toàn cục)
// ==========================================
const SPREADSHEET_ID = '1hc5alj_Vr05gKR0A9FdwLJgD-jQ1LSa6yX82TdOQOMs';
const SHEET_DANHSACH = 'DANHSACH';
const SHEET_DIEMDANH = 'DIEMDANH';
const TIMEZONE = 'Asia/Ho_Chi_Minh'; // Đã sửa từ Jakarta sang múi giờ Việt Nam

// ==========================================
// 2. HÀM CHÍNH - NHẬN YÊU CẦU TỪ ESP32
// ==========================================
function doGet(e) { 
  // Kiểm tra xem có dữ liệu gửi lên không
  if (!e || !e.parameter) {
    return ContentService.createTextOutput('Lỗi: Không có tham số truyền vào (No Parameters)');
  }

  // Mở file Google Sheets
  const sheetOpen = SpreadsheetApp.openById(SPREADSHEET_ID);

  // Lấy các tham số từ đường link URL do ESP32 gửi lên
  const sts = e.parameter.sts;
  const uid = e.parameter.uid ? stripQuotes(e.parameter.uid) : '';
  const name = e.parameter.name ? stripQuotes(e.parameter.name) : '';
  const inout = e.parameter.inout ? stripQuotes(e.parameter.inout) : '';

  // Lấy ngày và giờ hiện tại trên máy chủ
  const date = Utilities.formatDate(new Date(), TIMEZONE, 'dd/MM/yyyy');
  const time = Utilities.formatDate(new Date(), TIMEZONE, 'HH:mm:ss');

  // Bộ định tuyến (Router): Chuyển hướng xử lý dựa theo lệnh 'sts'
  switch (sts) {
    case 'read':
      return handleReadData(sheetOpen);

    case 'writeuid':
      return handleWriteUID(sheetOpen, uid);

    case 'writelog':
      return handleWriteLog(sheetOpen, date, time, uid, name, inout);

    default:
      return ContentService.createTextOutput('Lỗi: Lệnh (sts) không được hỗ trợ!');
  }
}

// ==========================================
// 3. CÁC HÀM XỬ LÝ RIÊNG BIỆT
// ==========================================

// Hàm 1: Gửi danh sách sinh viên cho ESP32
function handleReadData(sheetOpen) {
  const sheet = sheetOpen.getSheetByName(SHEET_DANHSACH);
  
  // Đọc dữ liệu từ cột A đến C, dòng 2 đến 11 (Tối đa 10 sinh viên)
  const data = sheet.getRange('A2:C11').getDisplayValues();
  return ContentService.createTextOutput(data);
}

// Hàm 2: Ghi thẻ lạ lên Sheet (Chế độ đăng ký thẻ)
function handleWriteUID(sheetOpen, uid) {
  if (!uid) return ContentService.createTextOutput('Lỗi: Không tìm thấy mã UID');

  const sheet = sheetOpen.getSheetByName(SHEET_DANHSACH);
  
  // Ghi mã UID vừa quẹt vào ô F1 để bạn dễ dàng copy
  sheet.getRange('F1').setValue(uid);
  return ContentService.createTextOutput('Success: Da ghi UID the moi vao o F1');
}

// Hàm 3: Ghi lịch sử điểm danh (Chế độ điểm danh)
function handleWriteLog(sheetOpen, date, time, uid, name, inout) {
  const sheet = sheetOpen.getSheetByName(SHEET_DIEMDANH);
  
  // Gom dữ liệu thành 1 hàng ngang (Mảng)
  const logData = [date, time, uid, name, inout];

  // Chèn 1 dòng trống mới lên đầu bảng (dòng 2)
  sheet.insertRows(2);
  
  // Ghi dữ liệu vào dòng trống đó (Bắt đầu từ cột 1, chiếm 1 hàng, chiều dài bằng số dữ liệu)
  sheet.getRange(2, 1, 1, logData.length).setValues([logData]);

  return ContentService.createTextOutput('Success: Da diem danh thanh cong');
}

// ==========================================
// 4. HÀM TIỆN ÍCH BỔ TRỢ
// ==========================================
function stripQuotes(value) {
  return value.replace(/^["']|['"]$/g, ""); // Xóa bỏ dấu ngoặc kép thừa nếu có
}