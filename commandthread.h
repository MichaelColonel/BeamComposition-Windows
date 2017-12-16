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

#pragma once

#include <queue>
#include <string>
#include "acquisitionthread.h"

#define COMMAND_SIZE 4

class CommandThread : public AcquireThread {
    Q_OBJECT
public:
    explicit CommandThread(QObject* parent = 0);
    virtual ~CommandThread();
    void initiate(int delay);
    void writeCommand( const char* cmd, size_t cmd_size = COMMAND_SIZE);

signals:
    void signalExternalSignal();
    void signalMovementFinished();
    void signalNewBatchState(bool);
    void signalStatusBarMessage(QString);
    void signalDeviceAnswer(QString);

protected:
    virtual void run();

    std::queue< std::string > commands;
    char* buffer;
};
