#include "Textbook.h"
#include <iostream>
#include <string>
Textbook::Textbook() :Book()
{
	subject = "";
	course = "";
}

Textbook::Textbook(string title, string author, string publisher, string ISBN, string id, double price, int stock, int availableCount, string subject, string course) :Book(title, author, publisher, ISBN, id, price, stock, availableCount)
{
	this->subject = subject;
	this->course = course;
}

string Textbook::getSubject()
{
	return subject;
}

string Textbook::getCourse()
{
	return course;
}

void Textbook::setSubject(string subject)
{
	this->subject = subject;
}

void Textbook::setCourse(string course)
{
	this->course = course;
}

void Textbook::display()
{
	Book::display();
	cout << "subject:" << subject << endl;
	cout << "course:" << course << endl;
}

