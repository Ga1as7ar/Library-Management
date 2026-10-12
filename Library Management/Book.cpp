#include "Book.h"
#include<iostream>
#include "Student.h"
Book::Book()
{
	title = "";
	author = "";
	publisher = "";
	ISBN = "";
	id = "";
	price = 0;
	stock = 0;
	availableCount = 0;
}

Book::Book(string title, string author, string publisher, string ISBN, string id, double price, int stock, int availableCount)//重载构造函数
{
	this->title = title;
	this->author = author;
	this->publisher = publisher;
	this->ISBN = ISBN;
	this->id = id;
	this->price = price;
	this->stock = stock;
	this->availableCount = availableCount;
}

void Book::display()
{
	cout << "title:" << title << endl;
	cout << "author:" << author << endl;
	cout << "publisher:" << publisher << endl;
	cout << "ISBN:" << ISBN << endl;
	cout << "id:" << id << endl;
	cout << "price:" << price << endl;
	if(availableCount>0){
		cout << "availableCount:" << availableCount << endl;
	}
	else{
		cout << "This book is currently not available for borrowing." << endl;
	}
}

string Book::getTitle()
{
	return title;
}

string Book::getAuthor()
{
	return author;
}

string Book::getPublisher()
{
	return publisher;
}

string Book::getISBN()
{
	return ISBN;
}

string Book::getId()
{
	return id;
}

double Book::getPrice()
{
	return price;
}

bool Book::canBorrow()
{
	return availableCount>0;
}

void Book::borrowOne()
{
	if (canBorrow()) {
		availableCount--;
		return;
	}
	else {
		cout << "Error: No available copies to borrow." << endl;
	}
}
	
void Book::returnOne()
{
	if(availableCount<stock){
		availableCount++;
	}
	else{
		cout << "Error: availableCount cannot exceed stock." << endl;
	}
}

void Book::setTitle(string title)
{
	this->title = title;
}

void Book::setAuthor(string author)
{
	this->author = author;
}

void Book::setPublisher(string publisher)
{
	this->publisher = publisher;
}

void Book::setISBN(string ISBN)
{
	this->ISBN = ISBN;
}

void Book::setId(string id)
{
	this->id = id;
}

void Book::setPrice(double price)
{
	this->price = price;
}
