#pragma once
#include"Book.h"
#include<string>
#include<vector>
using namespace std;
class Student
{
private:
	int studentid;
	string name;
	int BorrowedCount;
	vector<Book*> BorrowedBooks; // 存储借阅的书籍指针
public:
	Student();
	Student(int studentid, string name);

	int getStudentid();
	string getName();
	int getBorrowedCount();

	void setStudentid(int studentid);
	void setName(string name);
	
	void borrowBook(Book& book);
	void returnBook(Book& book);
	void showBorrowedBooks(); // 显示借阅的书籍列表

};

