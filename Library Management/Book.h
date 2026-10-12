#pragma once
#include<string>
using namespace std;
class Book
{
private:
	double price;
	int stock;
	int availableCount;
	string title;
	string author;
	string publisher;
	string ISBN;
	string id;
public:
	Book();
	Book(string title, string author, string publisher, string ISBN, string id, double price, int stock, int availableCount);

	string getTitle();
	string getAuthor();
	string getPublisher();
	string getISBN();
	string getId();
	double getPrice();

	bool canBorrow();
	void borrowOne();
	void returnOne();

	void setTitle(string title);
	void setAuthor(string author);
	void setPublisher(string publisher);
	void setISBN(string ISBN);
	void setId(string id);
	void setPrice(double price);

	virtual void display();
};

