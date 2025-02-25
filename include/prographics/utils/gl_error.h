#pragma once
#include <QOpenGLFunctions>
#include <QString>
#include <QDebug>

class GLErrorHandler {
public:
    /**
     * @brief 检查OpenGL错误
     * @param operation 当前执行的操作名称
     * @param file 源文件名
     * @param line 行号
     * @return bool 是否存在错误
     */
    static bool checkError(const char* operation, const char* file, int line) {
        GLenum error;
        bool hasError = false;

        while ((error = glGetError()) != GL_NO_ERROR) {
            QString errorMsg;
            switch (error) {
                case GL_INVALID_ENUM:
                    errorMsg = "GL_INVALID_ENUM";
                break;
                case GL_INVALID_VALUE:
                    errorMsg = "GL_INVALID_VALUE";
                break;
                case GL_INVALID_OPERATION:
                    errorMsg = "GL_INVALID_OPERATION";
                break;
                case GL_OUT_OF_MEMORY:
                    errorMsg = "GL_OUT_OF_MEMORY";
                break;
                default:
                    errorMsg = QString("Unknown error: 0x%1").arg(error, 0, 16);
            }

            qCritical() << QString("OpenGL Error in %1 at %2:%3 - %4: %5")
                           .arg(operation)
                           .arg(file)
                           .arg(line)
                           .arg(error)
                           .arg(errorMsg);
            hasError = true;
        }
        return hasError;
    }
};

// 方便使用的宏
#define GL_CHECK(operation) GLErrorHandler::checkError(operation, __FILE__, __LINE__)
