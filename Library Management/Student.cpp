#include "Student.h"//里面包含Book.h所以不需要额外再加include
#include<iostream>
Student::Student() {
	studentid = 0;
	name = "";
	BorrowedCount = 0;
}

Student::Student(int studentid, string name)
{
	this->studentid = studentid;
	this->name = name;
	BorrowedCount = 0;
}

int Student::getStudentid()
{
	return studentid;
}

string Student::getName()
{
	return name;
}

int Student::getBorrowedCount()
{
	return BorrowedCount;
}

void Student::setStudentid(int studentid)
{
	this->studentid = studentid;
}

void Student::setName(string name)
{
	this->name = name;
}

void Student::borrowBook(Book& book)//依赖关系 用&表示需要修改返回到Book里面的值 如果没有& 那么则无法返回到Book里
{
	if (!book.canBorrow()) {
				cout << "This book is not available for borrowing." << endl;
				return;
	}
	else {
		book.borrowOne();
		cout << "Book borrowed successfully." << endl;
		BorrowedBooks.push_back(&book); // 将书籍指针添加到BorrowedBooks中
		BorrowedCount++;
		return;
	}
}

void Student::returnBook(Book& book)
{
	for(auto it=BorrowedBooks.begin(); it!=BorrowedBooks.end(); ++it) {
		if(*it == &book) {
			book.returnOne();
			BorrowedBooks.erase(it); // 从BorrowedBooks中移除书籍指针
			BorrowedCount--;
			cout << "Book returned successfully." << endl;
			return;
		}
	}
	cout << "This book was not borrowed by the student." << endl;
}

void Student::showBorrowedBooks()
{
	if(BorrowedBooks.empty()) {
		cout << "No books borrowed." << endl;
		return;
	}
	cout << getName()<<" Borrowed Books:" << endl;
	for(Book* book:BorrowedBooks){
		cout << "Title: " << book->getTitle() << endl;
		cout << "----------------------------" << endl;
	}
}
