#!/usr/bin/env python


class FileHelper(object):
    _useLogger = True
    msgToLog = []
    blockLog = False
    logFilePath = "logs\\Logger.txt"

    @classmethod
    def isLoggerON(cls):
        """ generated source for method isLoggerON """
        return cls._useLogger

    @classmethod
    def setBlockLogON(cls):
        """ generated source for method setBlockLogON """
        cls.blockLog = True

    @classmethod
    def setBlockLogOFF(cls):
        """ generated source for method setBlockLogOFF """
        cls.blockLog = False
        if cls.msgToLog is not None and len(cls.msgToLog) > 0:
            writer = open(cls.logFilePath, 'w')
            for msg in cls.msgToLog:
                writer.write(msg)
                writer.write('\n')
            writer.flush()
            writer.close()

    @classmethod
    def seLoggerON(cls):
        """ generated source for method seLoggerON """
        cls._useLogger = True

    @classmethod
    def seLoggerOFF(cls):
        """ generated source for method seLoggerOFF """
        cls._useLogger = False

    #
    # 	 * Read all text from a file
    # 	 * @param file's Path
    # 	 * @return a list of strings that represent file lines
    #
    @classmethod
    def readAllLinesFromFile(cls, filePath):
        """ generated source for method readAllLinesFromFile_0 """
        try:
            lines = []
            f = open(filePath, 'r+')
            lines = f.readlines()
            f.close()
            return lines
        except Exception:
            return None

    @classmethod
    def logline(cls, message):
        """ generated source for method logline """
        if cls._useLogger:
            if not cls.blockLog:
                writer = open(cls.logFilePath, 'w')
                writer.write('\n')
                writer.write(message)
                writer.flush()
                writer.close()
                cls.msgToLog = []
            else:
                cls.msgToLog.append(message)