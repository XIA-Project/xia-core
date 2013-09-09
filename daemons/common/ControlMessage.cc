#include "ControlMessage.hh"

ControlMessage::ControlMessage(int type)
{
    clear();
    append(type);
}

ControlMessage::ControlMessage(std::string s)
{
    clear();
    _msg = s;
}

void ControlMessage::clear()
{
    _msg = "";
    _pos = 0;
}

size_t ControlMessage::length() const
{
    return _msg.length();
}

size_t ControlMessage::size() const
{
    return _msg.size();
}

const std::string ControlMessage::str() const
{
    return _msg;
}

const char *ControlMessage::c_str() const
{
    return _msg.c_str();
}

void ControlMessage::append(std::string s)
{
    _msg.append(s);
	_msg.append("^");
}

void ControlMessage::append(int n)
{
    stringstream out;
    out << n;
    append(out.str());
}

void ControlMessage::seek(int pos)
{
    _pos = pos;
}

int ControlMessage::read(std::string &s)
{
    size_t r = _msg.find("^", _pos);

    if (r == string::npos)
        return -1;

    s = _msg.substr(_pos, r - _pos);
    _pos = r + 1;

    return _pos;
}

int ControlMessage::read(int &n)
{
    string s;
    int r;

    r = read(s);
    n = atoi(s.c_str());

    return r;
}
