#pragma once
#include"Book.h"
class Textbook: public Book
{
	private:
		string subject;
		string course;
	public:
		Textbook();
		Textbook(string title, string author, string publisher, string ISBN, string id, double price, int stock, int availableCount, string subject, string course);
		string getSubject();
		string getCourse();
		void setSubject(string subject);
		void setCourse(string course);

		void display() override;
};

