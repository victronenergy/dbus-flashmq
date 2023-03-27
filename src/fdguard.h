#ifndef FDGUARD_H
#define FDGUARD_H


class FdGuard
{
    int fd = -1;
public:
    FdGuard(int fd);
    ~FdGuard();
    int get() const;
};

#endif // FDGUARD_H
