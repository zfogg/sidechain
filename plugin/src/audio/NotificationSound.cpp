#include "NotificationSound.h"
#include "../util/Log.h"
#include <juce_audio_formats/juce_audio_formats.h>

// Static members
std::unique_ptr<juce::AudioDeviceManager> NotificationSound::audioDeviceManager;
std::unique_ptr<juce::AudioSourcePlayer> NotificationSound::audioSourcePlayer;
std::unique_ptr<juce::AudioSource> NotificationSound::beepSource;

juce::AudioDeviceManager *NotificationSound::getAudioDeviceManager() {
  if (!audioDeviceManager) {
    audioDeviceManager = std::make_unique<juce::AudioDeviceManager>();
    audioSourcePlayer = std::make_unique<juce::AudioSourcePlayer>();

    // Initialize audio device (use default device)
    juce::String error = audioDeviceManager->initialise(0, 2, nullptr, true);
    if (error.isNotEmpty()) {
      Log::warn("NotificationSound: Failed to initialize audio device: " + error);
      return nullptr;
    }

    audioDeviceManager->addAudioCallback(audioSourcePlayer.get());
  }

  return audioDeviceManager.get();
}

bool NotificationSound::tryPlaySystemSound() {
  juce::File soundFile;

#if JUCE_MAC
  // macOS system sounds
  juce::File systemSoundsDir("/System/Library/Sounds");
  juce::StringArray soundNames = {"Glass.aiff", "Basso.aiff",  "Blow.aiff",      "Bottle.aiff", "Frog.aiff",
                                  "Funk.aiff",  "Hero.aiff",   "Morse.aiff",     "Ping.aiff",   "Pop.aiff",
                                  "Purr.aiff",  "Sosumi.aiff", "Submarine.aiff", "Tink.aiff"};

  for (const auto &name : soundNames) {
    soundFile = systemSoundsDir.getChildFile(name);
    if (soundFile.existsAsFile())
      break;
  }
#elif JUCE_WINDOWS
  // Windows system sounds - try multiple locations and common sound files
  juce::StringArray searchPaths = {
      "C:\\Windows\\Media", "C:\\Windows\\System32",
      juce::File::getSpecialLocation(juce::File::windowsSystemDirectory).getFullPathName() + "\\Media"};

  juce::StringArray soundNames = {"Windows Notify.wav", // Windows 10/11 default notification
                                  "Windows Notify System Generic.wav",
                                  "notify.wav",
                                  "Windows Message Nudge.wav",
                                  "Windows Logon.wav", // Alternative notification sounds
                                  "Windows Logoff.wav",
                                  "Windows Ding.wav",
                                  "chimes.wav", // Classic Windows sounds
                                  "chord.wav",
                                  "notify.wav"};

  for (const auto &path : searchPaths) {
    juce::File mediaDir(path);
    for (const auto &name : soundNames) {
      soundFile = mediaDir.getChildFile(name);
      if (soundFile.existsAsFile())
        break;
    }

    if (soundFile.existsAsFile())
      break;
  }
#elif JUCE_LINUX
  // Linux system sounds (various locations)
  juce::StringArray searchPaths = {"/usr/share/sounds/freedesktop/stereo/", "/usr/share/sounds/",
                                   "/usr/share/sounds/gnome/default/alerts/", "~/.local/share/sounds/"};

  juce::StringArray soundNames = {"message.ogg",      "message.wav", "notification.ogg",
                                  "notification.wav", "bell.ogg",    "bell.wav"};

  for (const auto &path : searchPaths) {
    juce::File searchDir(
        path.startsWith("~")
            ? juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(path.substring(2))
            : juce::File(path));

    for (const auto &name : soundNames) {
      soundFile = searchDir.getChildFile(name);
      if (soundFile.existsAsFile())
        break;
    }

    if (soundFile.existsAsFile())
      break;
  }
#endif

  if (!soundFile.existsAsFile())
    return false;

  // Try to load and play the system sound file
  auto *deviceManager = getAudioDeviceManager();
  if (!deviceManager)
    return false;

  juce::AudioFormatManager formatManager;
  formatManager.registerBasicFormats();

  std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(soundFile));
  if (!reader) {
    Log::debug("NotificationSound: Could not read system sound file: " + soundFile.getFullPathName());
    return false;
  }

  // Get duration before moving reader
  double duration = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;

  // Create audio source from the file (reader is moved into the source)
  beepSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);

  audioSourcePlayer->setSource(beepSource.get());

  // Schedule cleanup after playback
  juce::Timer::callAfterDelay(static_cast<int>(duration * 1000 + 100), []() {
    if (audioSourcePlayer)
      audioSourcePlayer->setSource(nullptr);
    beepSource.reset();
  });

  Log::debug("NotificationSound: Playing system sound: " + soundFile.getFileName());
  return true;
}

// Helper class to play audio from a buffer
class BufferAudioSource : public juce::AudioSource {
public:
  BufferAudioSource(const juce::AudioBuffer<float> &buf) : buffer(buf), position(0) {}

  void prepareToPlay(int, double) override {
    position = 0;
  }
  void releaseResources() override {}

  void getNextAudioBlock(const juce::AudioSourceChannelInfo &info) override {
    if (position >= buffer.getNumSamples()) {
      info.clearActiveBufferRegion();
      return;
    }

    int samplesToCopy = juce::jmin(info.numSamples, buffer.getNumSamples() - position);
    for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch) {
      info.buffer->copyFrom(ch, info.startSample, buffer, ch % buffer.getNumChannels(), position, samplesToCopy);
    }

    if (samplesToCopy < info.numSamples)
      info.buffer->clear(info.startSample + samplesToCopy, info.numSamples - samplesToCopy);

    position += samplesToCopy;
  }

private:
  const juce::AudioBuffer<float> buffer;
  int position;
};

void NotificationSound::playGeneratedBeep() {
  auto *deviceManager = getAudioDeviceManager();
  if (!deviceManager)
    return;

  // Generate a simple beep: 800Hz sine wave, 100ms duration, 44.1kHz sample
  // rate
  const double sampleRate = 44100.0;
  const double frequency = 800.0;
  const double duration = 0.1; // 100ms
  const int numSamples = static_cast<int>(sampleRate * duration);
  const double amplitude = 0.3; // 30% volume

  juce::AudioBuffer<float> buffer(1, numSamples); // Mono

  for (int i = 0; i < numSamples; ++i) {
    // Generate sine wave with fade in/out to avoid clicks
    double phase = 2.0 * juce::MathConstants<double>::pi * frequency * i / sampleRate;
    double sample = std::sin(phase) * amplitude;

    // Apply fade in/out (first and last 10ms)
    double fadeLength = sampleRate * 0.01; // 10ms
    if (i < fadeLength)
      sample *= (i / fadeLength);
    else if (i > numSamples - fadeLength)
      sample *= ((numSamples - i) / fadeLength);

    buffer.setSample(0, i, static_cast<float>(sample));
  }

  // Create source from buffer
  beepSource = std::make_unique<BufferAudioSource>(buffer);

  audioSourcePlayer->setSource(beepSource.get());

  // Auto-cleanup after playback
  scheduleCleanup();

  Log::debug("NotificationSound: Playing generated beep");
}

void NotificationSound::playBeep() {
  // Try system sound first, fall back to generated beep
  if (!tryPlaySystemSound()) {
    playGeneratedBeep();
  }
}

void NotificationSound::scheduleCleanup() {
  // Schedule cleanup after playback completes (100ms + 50ms buffer)
  juce::Timer::callAfterDelay(150, []() {
    if (audioSourcePlayer)
      audioSourcePlayer->setSource(nullptr);
    // beepSource will be cleaned up when playBeep() is called again or on
    // shutdown
  });
}
