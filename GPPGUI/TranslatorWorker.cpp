#include "TranslatorWorker.h"
#include <QThread>
#include <QTimer>

import std;
import ITranslator;

class GUIController : public IController
{

public:
    virtual void makeBar(int totalSentences, int totalThreads) override
    {
        this->flush();
        std::lock_guard<std::mutex> lock(_mutex);
        Q_EMIT _worker->makeBarSignal(totalSentences, totalThreads);
    }

    virtual void writeLog(const std::string& log) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _log += log;
    }

    virtual void addThreadNum() override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        Q_EMIT _worker->addThreadNumSignal();
    }

    virtual void reduceThreadNum() override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        Q_EMIT _worker->reduceThreadNumSignal();
    }

    virtual void updateBar(int ticks) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _progress += ticks;
    }

    virtual bool shouldStop() override
    {
        return _worker->getShouldStop();
    }

    virtual void flush() override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_log.isEmpty()) {
            Q_EMIT _worker->writeLogSignal(_log);
            _log.clear();
        }
        Q_EMIT _worker->updateBarSignal(_progress);
        _progress = 0;
    }

    explicit GUIController(TranslatorWorker* worker)
	    : _worker(worker)
    {
        _log.reserve(1024 * 1024);
        _flushThread = std::thread([this]()
            {
                while (_controlling) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    flush();
                }
            });
    }

    virtual ~GUIController() override
    {
        _controlling = false;
        if (_flushThread.joinable()) {
            _flushThread.join();
        }
    }

private:
    std::mutex _mutex;
    QString _log;
    std::thread _flushThread;
    TranslatorWorker* _worker;
    bool _controlling = true;
    int _progress = 0;
};

TranslatorWorker::TranslatorWorker(const fs::path& projectDir, QObject* parent)
    : QObject(parent), _projectDir(projectDir)
{
}

void TranslatorWorker::doTranslation()
{
    _shouldStop = false;

    const auto controller = std::make_shared<GUIController>(this);

    try {

        const std::unique_ptr<ITranslator> translator = createTranslator(_projectDir, controller);
        if (!translator) {
            Q_EMIT translationFinished(-1);
            return;
        }
        translator->run();

    }
    catch (const std::system_error& e) {
        controller->flush();
        Q_EMIT writeLogSignal("[系统错误] " + QString::fromStdString(e.what()));
        Q_EMIT translationFinished(-2);
        return;
    }
    catch (const std::invalid_argument& e) {
        controller->flush();
        Q_EMIT writeLogSignal("[参数错误] " + QString::fromStdString(e.what()));
        Q_EMIT translationFinished(-2);
        return;
    }
    catch (const std::runtime_error& e) {
        controller->flush();
        Q_EMIT writeLogSignal("[运行时错误] " + QString::fromStdString(e.what()));
        Q_EMIT translationFinished(-2);
        return;
    }
    catch (const std::exception& e) {
        controller->flush();
        Q_EMIT writeLogSignal("[标准错误] " + QString::fromStdString(e.what()));
        Q_EMIT translationFinished(-2);
        return;
    }
    catch (...) {
        controller->flush();
        Q_EMIT writeLogSignal("[未知错误]");
        Q_EMIT translationFinished(-2);
        return;
    }

    controller->writeLog(std::string("翻译任务") + (_shouldStop ? "已停止" : "已完成") + "。");
    controller->flush();
    Q_EMIT translationFinished(_shouldStop ? 1 : 0);
}

void TranslatorWorker::stopTranslation()
{
    _shouldStop = true;
}
