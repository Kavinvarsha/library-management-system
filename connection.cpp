#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <vector>

using namespace std;

SQLHENV env = NULL;
SQLHDBC dbc = NULL;
string currentUserRole;


void showError(const char* fn, SQLHANDLE handle, SQLSMALLINT type) {
    SQLINTEGER i = 0, native;
    SQLCHAR state[7], text[512];
    SQLSMALLINT len;
    while (SQLGetDiagRec(type, handle, ++i, state, &native, text, sizeof(text), &len) == SQL_SUCCESS)
        cerr << fn << " error: " << state << " " << native << " " << text << "\n";
}


bool exists(const string& sql) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt); return false;
    }
    SQLRETURN r = SQLFetch(stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO);
}


bool execSQL(const string& sql) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    SQLRETURN ret = SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) showError("execSQL", stmt, SQL_HANDLE_STMT);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return SQL_SUCCEEDED(ret);
}

bool querySingleInt(const string& sql, int &out) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLRETURN r = SQLFetch(stmt);
    if (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO) {
        SQLGetData(stmt, 1, SQL_C_SLONG, &out, 0, NULL);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return true;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return false;
}


bool querySingleDouble(const string& sql, double &out) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    SQLRETURN r = SQLFetch(stmt);
    if (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO) {
        SQLGetData(stmt, 1, SQL_C_DOUBLE, &out, 0, NULL);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return true;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return false;
}


bool login() {
    string username, password;
    cout << "\n=== Login ===\nUsername: ";
    getline(cin, username);
    cout << "Password: ";
    getline(cin, password);

    string sql = "SELECT Role FROM Users WHERE Username='" + username + "' AND Password='" + password + "'";
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        showError("Login", stmt, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLCHAR role[64];
    if (SQLFetch(stmt) == SQL_SUCCESS) {
        SQLGetData(stmt, 1, SQL_C_CHAR, role, sizeof(role), NULL);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        currentUserRole = (char*)role;
        cout << "Login successful. Role: " << currentUserRole << "\n";
        return true;
    } else {
        cout << "Invalid credentials.\n";
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
}

void addBook() {
    string title, author, genre, publisher, isbn, edition, rack, language;
    int year; double price;

    cout << "Title: "; getline(cin, title);
    cout << "Author: "; getline(cin, author);
    cout << "Genre: "; getline(cin, genre);
    cout << "Publisher: "; getline(cin, publisher);
    cout << "ISBN (unique): "; getline(cin, isbn);

    if (exists("SELECT * FROM Books WHERE ISBN='" + isbn + "'")) {
        cout << "ISBN already exists.\n"; return;
    }

    cout << "Edition: "; getline(cin, edition);
    cout << "Year: "; cin >> year; cin.ignore();
    cout << "Price: "; cin >> price; cin.ignore();
    cout << "Rack: "; getline(cin, rack);
    cout << "Language: "; getline(cin, language);

    string sql = "INSERT INTO Books (Title, Author, Genre, Publisher, ISBN, Edition, PublishedYear, Price, RackLocation, Language, Availability) "
                 "VALUES ('" + title + "','" + author + "','" + genre + "','" + publisher + "','" + isbn + "','" + edition + "'," +
                 to_string(year) + "," + to_string(price) + ",'" + rack + "','" + language + "', 1)";
    cout << (execSQL(sql) ? "Book added successfully.\n" : "Failed to add book.\n");
}

void updateBook() {
    string isbn;
    cout << "Enter ISBN to update: "; getline(cin, isbn);

    if (!exists("SELECT * FROM Books WHERE ISBN='" + isbn + "'")) {
        cout << "Book not found.\n"; return;
    }

    string title, author;
    cout << "New Title (leave blank to skip): "; getline(cin, title);
    cout << "New Author (leave blank to skip): "; getline(cin, author);

    string sql = "UPDATE Books SET ";
    bool needComma = false;
    if (!title.empty()) { sql += "Title='" + title + "'"; needComma = true; }
    if (!author.empty()) { if (needComma) sql += ", "; sql += "Author='" + author + "'"; }
    sql += " WHERE ISBN='" + isbn + "'";

    cout << (execSQL(sql) ? "Book updated.\n" : "Update failed.\n");
}

void deleteBook() {
    string isbn;
    cout << "Enter ISBN to delete: "; getline(cin, isbn);

    if (!exists("SELECT * FROM Books WHERE ISBN='" + isbn + "'")) {
        cout << "Book not found.\n"; return;
    }

    if (exists("SELECT * FROM Transactions WHERE BookID=(SELECT BookID FROM Books WHERE ISBN='" + isbn + "') AND ReturnDate IS NULL")) {
        cout << "Book currently issued, cannot delete.\n"; return;
    }

    cout << (execSQL("DELETE FROM Books WHERE ISBN='" + isbn + "'") ? "Book deleted.\n" : "Delete failed.\n");
}

void viewBooks() {
    int pageSize = 5, page = 0;
    string choice;
    do {
        int offset = page * pageSize;
        string sql = "SELECT BookID, Title, Author, ISBN, Availability FROM Books ORDER BY Title "
                     "OFFSET " + to_string(offset) + " ROWS FETCH NEXT " + to_string(pageSize) + " ROWS ONLY";

        SQLHSTMT stmt;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
            cout << "\nPage " << page + 1 << "\nID\tTitle\tAuthor\tISBN\tAvailability\n";
            while (SQLFetch(stmt) == SQL_SUCCESS) {
                int id, avail;
                char title[256], author[200], isbn[64];
                memset(title,0,sizeof(title)); memset(author,0,sizeof(author)); memset(isbn,0,sizeof(isbn));
                SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
                SQLGetData(stmt, 2, SQL_C_CHAR, title, sizeof(title), NULL);
                SQLGetData(stmt, 3, SQL_C_CHAR, author, sizeof(author), NULL);
                SQLGetData(stmt, 4, SQL_C_CHAR, isbn, sizeof(isbn), NULL);
                SQLGetData(stmt, 5, SQL_C_LONG, &avail, 0, NULL);
                cout << id << "\t" << title << "\t" << author << "\t" << isbn << "\t" << (avail ? "Yes" : "No") << "\n";
            }
        }
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);

        cout << "[N]ext, [P]revious, [Q]uit: ";
        getline(cin, choice);
        if (choice == "N" || choice == "n") page++;
        else if ((choice == "P" || choice == "p") && page > 0) page--;
    } while (choice != "Q" && choice != "q");
}

void searchBooks() {
    cout << "Enter keyword to search in Title/Author/Genre/ISBN: ";
    string keyword; getline(cin, keyword);

    string sql = "SELECT BookID, Title, Author, ISBN FROM Books WHERE "
                 "Title LIKE '%" + keyword + "%' OR Author LIKE '%" + keyword + "%' OR Genre LIKE '%" + keyword + "%' OR ISBN LIKE '%" + keyword + "%'";

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "ID\tTitle\tAuthor\tISBN\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int id;
            char title[256], author[200], isbn[64];
            memset(title,0,sizeof(title)); memset(author,0,sizeof(author)); memset(isbn,0,sizeof(isbn));
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, title, sizeof(title), NULL);
            SQLGetData(stmt, 3, SQL_C_CHAR, author, sizeof(author), NULL);
            SQLGetData(stmt, 4, SQL_C_CHAR, isbn, sizeof(isbn), NULL);
            cout << id << "\t" << title << "\t" << author << "\t" << isbn << "\n";
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

vector<string> splitCSVLine(const string &line) {
    vector<string> result;
    string field;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '"') {
            inQuotes = !inQuotes; // toggle state
        } else if (c == ',' && !inQuotes) {
            result.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    result.push_back(field);
    return result;
}




void bulkImportBooks() {
    if (currentUserRole != "Admin") {
        std::cout << "Access denied. Only administrators can bulk import books." << std::endl;
        return;
    }

    std::string filename = "book.csv";  // CSV file path
    std::cout << "\n--- Bulk Import Books ---" << std::endl;
    std::cout << "Attempting to import from: " << filename << std::endl;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header if present

    SQLHSTMT hStmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hStmt);

    SQLWCHAR insertQuery[] =
        L"INSERT INTO Books (Title, Author, Genre, Publisher, ISBN, Edition, PublishedYear, Price, RackLocation, Language) "
        L"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    SQLPrepareW(hStmt, insertQuery, SQL_NTS);

    int rowsImported = 0;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string title, author, genre, publisher, isbn, edition, yearStr, priceStr, rack, language;

        std::getline(ss, title, ',');
        std::getline(ss, author, ',');
        std::getline(ss, genre, ',');
        std::getline(ss, publisher, ',');
        std::getline(ss, isbn, ',');
        std::getline(ss, edition, ',');
        std::getline(ss, yearStr, ',');
        std::getline(ss, priceStr, ',');
        std::getline(ss, rack, ',');
        std::getline(ss, language);

        // Convert to wide-char
        SQLWCHAR wTitle[256], wAuthor[256], wGenre[101], wPublisher[201], wIsbn[21], wEdition[51], wRack[51], wLanguage[51];
        mbstowcs(wTitle, title.c_str(), 256);
        mbstowcs(wAuthor, author.c_str(), 256);
        mbstowcs(wGenre, genre.c_str(), 101);
        mbstowcs(wPublisher, publisher.c_str(), 201);
        mbstowcs(wIsbn, isbn.c_str(), 21);
        mbstowcs(wEdition, edition.c_str(), 51);
        mbstowcs(wRack, rack.c_str(), 51);
        mbstowcs(wLanguage, language.c_str(), 51);

        int year = std::stoi(yearStr);
        double price = std::stod(priceStr);

        // Bind parameters
        SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 200, 0, wTitle, 0, NULL);
        SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 200, 0, wAuthor, 0, NULL);
        SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 100, 0, wGenre, 0, NULL);
        SQLBindParameter(hStmt, 4, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 200, 0, wPublisher, 0, NULL);
        SQLBindParameter(hStmt, 5, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 20, 0, wIsbn, 0, NULL);
        SQLBindParameter(hStmt, 6, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 50, 0, wEdition, 0, NULL);
        SQLBindParameter(hStmt, 7, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &year, 0, NULL);
        SQLBindParameter(hStmt, 8, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE, 0, 2, &price, 0, NULL);
        SQLBindParameter(hStmt, 9, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 50, 0, wRack, 0, NULL);
        SQLBindParameter(hStmt, 10, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 50, 0, wLanguage, 0, NULL);

        if (SQL_SUCCEEDED(SQLExecute(hStmt))) {
            rowsImported++;
        } else {
            std::cerr << "Failed to import row: " << line << std::endl;
            showError("SQLExecute", hStmt, SQL_HANDLE_STMT);
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    std::cout << "Bulk import complete. " << rowsImported << " books were added." << std::endl;
}

void addMember() {
    string name, email, phone, address, type;
    cout << "Name: "; getline(cin, name);
    cout << "Email: "; getline(cin, email);
    cout << "Phone: "; getline(cin, phone);
    cout << "Address: "; getline(cin, address);
    cout << "Membership Type (Regular/Premium): "; getline(cin, type);

    if (exists("SELECT * FROM Members WHERE Email='" + email + "'")) { cout << "Email exists.\n"; return; }
    if (exists("SELECT * FROM Members WHERE Phone='" + phone + "'")) { cout << "Phone exists.\n"; return; }

    string sql = "INSERT INTO Members (Name, Email, Phone, Address, MembershipType, Status) "
                 "VALUES ('" + name + "','" + email + "','" + phone + "','" + address + "','" + type + "','Active')";
    cout << (execSQL(sql) ? "Member added.\n" : "Failed.\n");
}

void updateMember() {
    int id; cout << "Enter Member ID: "; cin >> id; cin.ignore();
    if (!exists("SELECT * FROM Members WHERE MemberID=" + to_string(id))) { cout << "Member not found.\n"; return; }

    string address, phone, type;
    cout << "New Address (blank to skip): "; getline(cin, address);
    cout << "New Phone (blank to skip): "; getline(cin, phone);
    cout << "New Type (blank to skip): "; getline(cin, type);

    string sql = "UPDATE Members SET "; bool needComma = false;
    if (!address.empty()) { sql += "Address='" + address + "'"; needComma = true; }
    if (!phone.empty()) { if (needComma) sql += ", "; sql += "Phone='" + phone + "'"; needComma = true; }
    if (!type.empty()) { if (needComma) sql += ", "; sql += "MembershipType='" + type + "'"; }
    sql += " WHERE MemberID=" + to_string(id);
    cout << (execSQL(sql) ? "Member updated.\n" : "Failed.\n");
}

void deleteMember() {
    int id; cout << "Enter Member ID: "; cin >> id; cin.ignore();
    if (!exists("SELECT * FROM Members WHERE MemberID=" + to_string(id))) { cout << "Member not found.\n"; return; }

    if (exists("SELECT * FROM Transactions WHERE MemberID=" + to_string(id) + " AND ReturnDate IS NULL")) {
        string sql = "UPDATE Members SET Status='Inactive' WHERE MemberID=" + to_string(id);
        cout << (execSQL(sql) ? "Member deactivated.\n" : "Failed.\n");
    } else {
        cout << (execSQL("DELETE FROM Members WHERE MemberID=" + to_string(id)) ? "Member deleted.\n" : "Failed.\n");
    }
}

void viewMembers() {
    int page = 0, pageSize = 5; string choice;
    do {
        int offset = page * pageSize;
        string sql = "SELECT MemberID, Name, Email, Phone, MembershipType, Status FROM Members "
                     "ORDER BY Name OFFSET " + to_string(offset) + " ROWS FETCH NEXT " + to_string(pageSize) + " ROWS ONLY";

        SQLHSTMT stmt; SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
            cout << "\nPage " << page + 1 << "\nID\tName\tEmail\tPhone\tType\tStatus\n";
            while (SQLFetch(stmt) == SQL_SUCCESS) {
                int id; char name[128], email[128], phone[32], type[32], status[16];
                memset(name,0,sizeof(name)); memset(email,0,sizeof(email)); memset(phone,0,sizeof(phone));
                memset(type,0,sizeof(type)); memset(status,0,sizeof(status));
                SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
                SQLGetData(stmt, 2, SQL_C_CHAR, name, sizeof(name), NULL);
                SQLGetData(stmt, 3, SQL_C_CHAR, email, sizeof(email), NULL);
                SQLGetData(stmt, 4, SQL_C_CHAR, phone, sizeof(phone), NULL);
                SQLGetData(stmt, 5, SQL_C_CHAR, type, sizeof(type), NULL);
                SQLGetData(stmt, 6, SQL_C_CHAR, status, sizeof(status), NULL);
                cout << id << "\t" << name << "\t" << email << "\t" << phone << "\t" << type << "\t" << status << "\n";
            }
        }
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        cout << "[N]ext, [P]revious, [Q]uit: "; getline(cin, choice);
        if (choice == "N" || choice == "n") page++;
        else if ((choice == "P" || choice == "p") && page > 0) page--;
    } while (choice != "Q" && choice != "q");
}

void searchMembers() {
    cout << "Enter keyword: "; string keyword; getline(cin, keyword);
    string sql = "SELECT MemberID, Name, Email, Phone, MembershipType, Status FROM Members "
                 "WHERE Name LIKE '%" + keyword + "%' OR Email LIKE '%" + keyword + "%' OR MembershipType LIKE '%" + keyword + "%'";

    SQLHSTMT stmt; SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "ID\tName\tEmail\tPhone\tType\tStatus\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int id; char name[128], email[128], phone[32], type[32], status[16];
            memset(name,0,sizeof(name)); memset(email,0,sizeof(email)); memset(phone,0,sizeof(phone));
            memset(type,0,sizeof(type)); memset(status,0,sizeof(status));
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, name, sizeof(name), NULL);
            SQLGetData(stmt, 3, SQL_C_CHAR, email, sizeof(email), NULL);
            SQLGetData(stmt, 4, SQL_C_CHAR, phone, sizeof(phone), NULL);
            SQLGetData(stmt, 5, SQL_C_CHAR, type, sizeof(type), NULL);
            SQLGetData(stmt, 6, SQL_C_CHAR, status, sizeof(status), NULL);
            cout << id << "\t" << name << "\t" << email << "\t" << phone << "\t" << type << "\t" << status << "\n";
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}


int getConfigInt(const string &col, int defaultVal) {
    int val = 0;
    if (querySingleInt("SELECT " + col + " FROM Config", val)) return val;
    return defaultVal;
}


double getFineRate() {
    double val = 0.0;
    if (querySingleDouble("SELECT FineRate FROM Config", val)) return val;
    return 5.0; 
}


int getActiveLoanCount(int memberID) {
    int cnt = 0;
    querySingleInt("SELECT COUNT(*) FROM Transactions WHERE MemberID=" + to_string(memberID) + " AND ReturnDate IS NULL", cnt);
    return cnt;
}

bool isBookAvailable(int bookID) {
    int avail = 0;
    if (querySingleInt("SELECT CAST(Availability AS INT) FROM Books WHERE BookID=" + to_string(bookID), avail)) {
        return avail == 1;
    }
    return false;
}


void issueBook() {
    int memberID, bookID;
    cout << "Enter MemberID: "; cin >> memberID;
    cout << "Enter BookID: "; cin >> bookID;
    cin.ignore();

    if (!exists("SELECT * FROM Members WHERE MemberID=" + to_string(memberID))) {
        cout << "Member not found.\n"; return;
    }
    if (!exists("SELECT * FROM Books WHERE BookID=" + to_string(bookID))) {
        cout << "Book not found.\n"; return;
    }

    if (!isBookAvailable(bookID)) {
        int firstResMember = 0;
        if (querySingleInt("SELECT TOP 1 MemberID FROM Reservations WHERE BookID=" + to_string(bookID) + " AND Status='Pending' ORDER BY ReservationDate", firstResMember)) {
            if (firstResMember != memberID) {
                cout << "Book not available and reserved by another member. Cannot issue.\n";
                return;
            }
        } else {
            cout << "Book currently not available.\n"; return;
        }
    }

    int maxBooks = getConfigInt("MaxBooks", 5);
    int activeLoans = getActiveLoanCount(memberID);
    if (activeLoans >= maxBooks) {
        cout << "Member has reached max allowed books (" << maxBooks << ").\n"; return;
    }

    string sqlInsert = "INSERT INTO Transactions (MemberID, BookID, IssueDate, DueDate) "
                       "VALUES (" + to_string(memberID) + "," + to_string(bookID) + ", GETDATE(), DATEADD(DAY, 14, GETDATE()))";
    if (!execSQL(sqlInsert)) {
        cout << "Failed to create transaction.\n"; return;
    }

    if (!execSQL("UPDATE Books SET Availability = 0 WHERE BookID=" + to_string(bookID))) {
        cout << "Warning: issued but failed to update book availability.\n";
    }

    if (exists("SELECT * FROM Reservations WHERE BookID=" + to_string(bookID) + " AND MemberID=" + to_string(memberID) + " AND Status='Pending'")) {
        execSQL("UPDATE Reservations SET Status='Completed' WHERE BookID=" + to_string(bookID) + " AND MemberID=" + to_string(memberID) + " AND Status='Pending'");
    }

    cout << "Book issued successfully. Due in 14 days.\n";
}


void returnBook() {
    int transID;
    cout << "Enter TransactionID: "; cin >> transID;
    cin.ignore();

    if (!exists("SELECT * FROM Transactions WHERE TransactionID=" + to_string(transID))) {
        cout << "Transaction not found.\n"; return;
    }
    if (exists("SELECT * FROM Transactions WHERE TransactionID=" + to_string(transID) + " AND ReturnDate IS NOT NULL")) {
        cout << "This transaction already marked returned.\n"; return;
    }

    double fineRate = getFineRate();
    string sqlUpdate =
        "UPDATE Transactions SET ReturnDate = GETDATE(), "
        "FineAmount = CASE WHEN GETDATE() > DueDate THEN DATEDIFF(DAY, DueDate, GETDATE()) * " + to_string(fineRate) + " ELSE 0 END "
        "WHERE TransactionID = " + to_string(transID);
    if (!execSQL(sqlUpdate)) {
        cout << "Failed to update return.\n"; return;
    }

    int bookID = 0;
    querySingleInt("SELECT BookID FROM Transactions WHERE TransactionID=" + to_string(transID), bookID);

    if (bookID == 0) {
        cout << "Warning: Could not find BookID for transaction.\n";
    } else {
        execSQL("UPDATE Books SET Availability = 1 WHERE BookID=" + to_string(bookID));

        int nextMember = 0;
        if (querySingleInt("SELECT TOP 1 MemberID FROM Reservations WHERE BookID=" + to_string(bookID) + " AND Status='Pending' ORDER BY ReservationDate", nextMember)) {
            if (execSQL("UPDATE Reservations SET Status='Completed' WHERE BookID=" + to_string(bookID) + " AND MemberID=" + to_string(nextMember) + " AND Status='Pending'")) {
                if (execSQL("INSERT INTO Transactions (MemberID, BookID, IssueDate, DueDate) VALUES (" + to_string(nextMember) + "," + to_string(bookID) + ", GETDATE(), DATEADD(DAY,14,GETDATE()))")) {
                    execSQL("UPDATE Books SET Availability = 0 WHERE BookID=" + to_string(bookID));
                    cout << "Book returned. It was automatically issued to MemberID " << nextMember << " from reservation queue.\n";
                } else {
                    cout << "Book returned. Reservation marked, but failed to auto-issue.\n";
                }
            }
        } else {
            cout << "Book returned and is now available.\n";
        }
    }
}

void reserveBook() {
    int memberID, bookID;
    cout << "Enter MemberID: "; cin >> memberID;
    cout << "Enter BookID: "; cin >> bookID;
    cin.ignore();

    if (!exists("SELECT * FROM Members WHERE MemberID=" + to_string(memberID))) {
        cout << "Member not found.\n"; return;
    }
    if (!exists("SELECT * FROM Books WHERE BookID=" + to_string(bookID))) {
        cout << "Book not found.\n"; return;
    }

    if (exists("SELECT * FROM Reservations WHERE MemberID=" + to_string(memberID) + " AND BookID=" + to_string(bookID) + " AND Status='Pending'")) {
        cout << "You already have a pending reservation for this book.\n"; return;
    }

    if (execSQL("INSERT INTO Reservations (MemberID, BookID, ReservationDate, Status) VALUES (" + to_string(memberID) + "," + to_string(bookID) + ", GETDATE(), 'Pending')")) {
        cout << "Reservation placed.\n";
    } else {
        cout << "Failed to reserve book.\n";
    }
}


void viewTransactionHistory() {
    cout << "\nView history by: 1) Member  2) Book\nChoice: ";
    string ch; getline(cin, ch);
    int id = 0;
    string where;
    if (ch == "1") {
        cout << "Enter MemberID: "; string s; getline(cin, s); id = stoi(s);
        where = "MemberID = " + to_string(id);
    } else {
        cout << "Enter BookID: "; string s; getline(cin, s); id = stoi(s);
        where = "BookID = " + to_string(id);
    }

    int page = 0, pageSize = 5;
    string nav;
    do {
        int offset = page * pageSize;
        string sql = "SELECT TransactionID, MemberID, BookID, IssueDate, DueDate, ReturnDate, FineAmount "
                     "FROM Transactions WHERE " + where + " ORDER BY IssueDate DESC "
                     "OFFSET " + to_string(offset) + " ROWS FETCH NEXT " + to_string(pageSize) + " ROWS ONLY";

        SQLHSTMT stmt; SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
            cout << "\nPage " << page + 1 << "\nTxID\tMemberID\tBookID\tIssueDate\tDueDate\tReturnDate\tFine\n";
            while (SQLFetch(stmt) == SQL_SUCCESS) {
                int txid=0, mid=0, bid=0; char issue[32] = {0}, due[32] = {0}, ret[32] = {0}; double fine=0.0;
                SQLGetData(stmt, 1, SQL_C_SLONG, &txid, 0, NULL);
                SQLGetData(stmt, 2, SQL_C_SLONG, &mid, 0, NULL);
                SQLGetData(stmt, 3, SQL_C_SLONG, &bid, 0, NULL);
                SQLGetData(stmt, 4, SQL_C_CHAR, issue, sizeof(issue), NULL);
                SQLGetData(stmt, 5, SQL_C_CHAR, due, sizeof(due), NULL);
                SQLGetData(stmt, 6, SQL_C_CHAR, ret, sizeof(ret), NULL);
                SQLGetData(stmt, 7, SQL_C_DOUBLE, &fine, 0, NULL);
                cout << txid << "\t" << mid << "\t\t" << bid << "\t" << issue << "\t" << due << "\t" << (strlen(ret)? ret : "NULL") << "\t" << fine << "\n";
            }
        } else {
            cout << "Failed to fetch history.\n";
        }
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);

        cout << "[N]ext, [P]revious, [Q]uit: "; getline(cin, nav);
        if (nav == "N" || nav == "n") page++;
        else if ((nav == "P" || nav == "p") && page > 0) page--;
    } while (nav != "Q" && nav != "q");
}


void exportToCSV(const string &sql, const string &filename) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) {
        cout << "Failed to allocate statement for export.\n"; return;
    }

    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        showError("ExportToCSV", stmt, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return;
    }

    ofstream file(filename);
    if (!file.is_open()) {
        cout << "Failed to open file " << filename << "\n";
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return;
    }

    
    SQLSMALLINT cols = 0;
    SQLNumResultCols(stmt, &cols);

    
    for (int i = 1; i <= cols; i++) {
        SQLCHAR colName[256];
        SQLSMALLINT nameLen;
        SQLDescribeCol(stmt, i, colName, sizeof(colName), &nameLen, NULL, NULL, NULL, NULL);
        file << (char*)colName;
        if (i < cols) file << ",";
    }
    file << "\n";

    
    while (SQLFetch(stmt) == SQL_SUCCESS) {
        for (int i = 1; i <= cols; i++) {
            char buf[1024] = {0};
            SQLLEN ind;
            SQLRETURN r = SQLGetData(stmt, i, SQL_C_CHAR, buf, sizeof(buf), &ind);
            if (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO) {
                if (ind == SQL_NULL_DATA) file << "";
                else {
       
                    string val = buf;
                    bool needQuotes = (val.find(',') != string::npos) || (val.find('"') != string::npos) || (val.find('\n') != string::npos);
                    if (needQuotes) {
                        
                        string escaped;
                        for (char c : val) {
                            if (c == '"') escaped += "\"\"";
                            else escaped += c;
                        }
                        file << "\"" << escaped << "\"";
                    } else {
                        file << val;
                    }
                }
            } else {
                file << "";
            }
            if (i < cols) file << ",";
        }
        file << "\n";
    }

    file.close();
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    cout << "Exported to " << filename << "\n";
}


void topIssuedBooks(bool doExport = false) {
    string sql =
        "SELECT TOP 10 B.Title, COUNT(T.TransactionID) AS IssueCount "
        "FROM Transactions T JOIN Books B ON T.BookID = B.BookID "
        "GROUP BY B.Title ORDER BY IssueCount DESC";

    SQLHSTMT stmt; SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "Top 10 Issued Books:\nTitle\tIssues\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            char title[256] = {0}; int cnt = 0;
            SQLGetData(stmt, 1, SQL_C_CHAR, title, sizeof(title), NULL);
            SQLGetData(stmt, 2, SQL_C_SLONG, &cnt, 0, NULL);
            cout << title << "\t" << cnt << "\n";
        }
    } else cout << "Failed to fetch top issued books.\n";
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (doExport) exportToCSV(sql, "top_10_issued_books.csv");
}


void mostActiveMembers(bool doExport = false) {
    string sql =
        "SELECT TOP 10 M.Name, COUNT(T.TransactionID) AS ActivityCount "
        "FROM Transactions T JOIN Members M ON T.MemberID = M.MemberID "
        "GROUP BY M.Name ORDER BY ActivityCount DESC";

    SQLHSTMT stmt; SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "Most Active Members:\nName\tTransactions\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            char name[128] = {0}; int cnt = 0;
            SQLGetData(stmt, 1, SQL_C_CHAR, name, sizeof(name), NULL);
            SQLGetData(stmt, 2, SQL_C_SLONG, &cnt, 0, NULL);
            cout << name << "\t" << cnt << "\n";
        }
    } else cout << "Failed to fetch active members.\n";
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (doExport) exportToCSV(sql, "most_active_members.csv");
}


void fineSummary(bool doExport = false) {
    string sql = "SELECT SUM(FineAmount) AS TotalFine, COUNT(*) AS TransactionsWithFine FROM Transactions WHERE FineAmount > 0";

    SQLHSTMT stmt; SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        double total = 0.0; int cnt = 0;
        SQLGetData(stmt, 1, SQL_C_DOUBLE, &total, 0, NULL);
        SQLGetData(stmt, 2, SQL_C_SLONG, &cnt, 0, NULL);
        cout << "Fine Collection Summary:\nTotal Fine Collected: " << total << "\nTransactions with Fine: " << cnt << "\n";
    } else cout << "Failed to fetch fine summary.\n";
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (doExport) exportToCSV("SELECT TransactionID, MemberID, BookID, IssueDate, DueDate, ReturnDate, FineAmount FROM Transactions WHERE FineAmount > 0", "fine_collection_details.csv");
}


void reportsMenu() {
    string choice;
    do {
        cout << "\n===== Reports Menu =====\n"
             << "1. View Top 10 Issued Books (and auto-export to top_10_issued_books.csv)\n"
             << "2. View Most Active Members (and auto-export to most_active_members.csv)\n"
             << "3. View Fine Collection Summary (and export details to fine_collection_details.csv)\n"
             << "4. Back\n> ";
        getline(cin, choice);

        if (choice == "1") topIssuedBooks(true);
        else if (choice == "2") mostActiveMembers(true);
        else if (choice == "3") fineSummary(true);
    } while (choice != "4");
}


void booksMenu();
void membersMenu();
void transactionsMenu();
void reportsMenuWrapper() { reportsMenu(); }

void booksMenu() {
    string choice;
    do {
        cout << "\n===== Books Menu =====\n1.Add\n2.Update\n3.Delete\n4.View\n5.Search\n6.Bulk Import (placeholder)\n7.Back\n> ";
        getline(cin, choice);
        if (choice=="1") addBook();
        else if(choice=="2") updateBook();
        else if(choice=="3") deleteBook();
        else if(choice=="4") viewBooks();
        else if(choice=="5") searchBooks();
        else if(choice=="6") bulkImportBooks();
    } while(choice!="7");
}

void membersMenu() {
    string choice;
    do {
        cout << "\n===== Members Menu =====\n1.Add\n2.Update\n3.Delete\n4.View\n5.Search\n6.Back\n> ";
        getline(cin, choice);
        if (choice=="1") addMember();
        else if (choice=="2") updateMember();
        else if (choice=="3") deleteMember();
        else if (choice=="4") viewMembers();
        else if (choice=="5") searchMembers();
    } while(choice!="6");
}

void transactionsMenu() {
    string choice;
    do {
        cout << "\n===== Transactions Menu =====\n1.Issue Book\n2.Return Book\n3.Reserve Book\n4.View Transaction History\n5.Back\n> ";
        getline(cin, choice);
        if (choice=="1") issueBook();
        else if (choice=="2") returnBook();
        else if (choice=="3") reserveBook();
        else if (choice=="4") viewTransactionHistory();
    } while(choice!="5");
}

void menu() {
    string choice;
    do {
        cout << "\n===== Main Menu =====\n1.Books Management\n2.Members Management\n3.Transactions\n4.Reports\n5.Exit\n> ";
        getline(cin, choice);
        if(choice=="1") booksMenu();
        else if(choice=="2") membersMenu();
        else if(choice=="3") transactionsMenu();
        else if(choice=="4") reportsMenu();
    } while(choice!="5");
}


int main() {
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);


    SQLCHAR connStr[] = "DRIVER={ODBC Driver 17 for SQL Server};SERVER=PSILENL076;DATABASE=librarydb;Trusted_Connection=Yes;";
    if (!SQL_SUCCEEDED(SQLDriverConnect(dbc, NULL, connStr, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_COMPLETE))) {
        showError("Connection", dbc, SQL_HANDLE_DBC);
        return 1;
    }

    if (!login()) {
        SQLDisconnect(dbc); SQLFreeHandle(SQL_HANDLE_DBC, dbc); SQLFreeHandle(SQL_HANDLE_ENV, env);
        return 0;
    }

    menu();

    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    return 0;
}


