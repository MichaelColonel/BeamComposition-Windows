/*
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include <QDataStream>
#include <QTextStream>
#include <QFile>

#include "writeprocess.h"

WriteDataProcess::WriteDataProcess( QFile* file, const DataList& d)
    :
    filerun(file),
    data(d)
{
}

WriteDataProcess::~WriteDataProcess()
{
}

void
WriteDataProcess::run()
{
    if (filerun && filerun->isOpen()) {
        QDataStream out(filerun);
//        for ( quint8 value : data) {
        for ( DataList::const_iterator it = data.begin(); it != data.end(); ++it) {
            const quint8& value = *it;
            out << value;
        }
        filerun->flush();
    }
}

WriteCountsProcess::WriteCountsProcess( QFile* file, const CountsList& c)
    :
    WriteDataProcess(file),
    counts(c)
{
}

WriteCountsProcess::~WriteCountsProcess()
{
}

void
WriteCountsProcess::run()
{
    if (filerun && filerun->isOpen()) {
        QTextStream out(filerun);
//        for ( const CountsArray& array : counts) {
        for ( CountsList::const_iterator it = counts.begin(); it != counts.end(); ++it) {
            const CountsArray& array = *it;
            out << array[0] << " " << array[1] << " ";
            out << array[2] << " " << array[3] << endl;
        }
    }
}
