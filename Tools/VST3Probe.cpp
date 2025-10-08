#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::StringArray args;
    for (int i = 0; i < argc; ++i)
        args.add (argv[i]);

    if (args.size() < 2)
    {
        juce::Logger::writeToLog ("Usage: VST3Probe <path-to-vst3>");
        return 1;
    }

    auto pluginPath = args[1];
    juce::Logger::writeToLog ("Probing VST3: " + pluginPath);

   juce::AudioPluginFormatManager formatManager;
   formatManager.addDefaultFormats();

    juce::Logger::writeToLog ("Known formats:");
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
        juce::Logger::writeToLog ("  " + formatManager.getFormat(i)->getName());

    juce::AudioPluginFormat* vst3Format = nullptr;
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        if (formatManager.getFormat(i)->getName().containsIgnoreCase ("VST3"))
        {
            vst3Format = formatManager.getFormat(i);
            break;
        }
    }

    if (vst3Format == nullptr)
    {
        juce::Logger::writeToLog ("No VST3 format available.");
        return 2;
    }

    juce::OwnedArray<juce::PluginDescription> descriptions;
    juce::String error;
   vst3Format->findAllTypesForFile (descriptions, pluginPath);

    for (auto* desc : descriptions)
    {
        juce::Logger::writeToLog ("Description: name=" + desc->name
                                   + ", uniqueId=" + juce::String (desc->uniqueId)
                                   + ", file=" + desc->fileOrIdentifier);
    }

    if (descriptions.isEmpty())
    {
        juce::Logger::writeToLog ("No plugin description found for path.");
        return 3;
    }

    auto instance = formatManager.createPluginInstance (*descriptions[0], 44100.0, 512, error);

    if (instance == nullptr)
    {
        juce::Logger::writeToLog ("Failed to create plugin instance: " + error);
        return 4;
    }

    juce::Logger::writeToLog ("Plugin instantiated successfully.");
    juce::Logger::writeToLog ("Name: " + instance->getName());
    juce::Logger::writeToLog ("Inputs: " + juce::String (instance->getTotalNumInputChannels())
                              + ", Outputs: " + juce::String (instance->getTotalNumOutputChannels()));

    instance->prepareToPlay (44100.0, 512);
    juce::AudioBuffer<float> buffer (instance->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    buffer.clear();

    instance->processBlock (buffer, midi);
    juce::Logger::writeToLog ("processBlock completed without error.");
    instance->releaseResources();

    return 0;
}
