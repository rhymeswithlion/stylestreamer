#include "engine/PromptStateClipboard.h"

namespace mrt::plugin
{
namespace
{

[[nodiscard]] juce::String memoryBlockToString (const juce::MemoryBlock& block)
{
    return juce::String::fromUTF8 (
        static_cast<const char*> (block.getData()),
        static_cast<int> (block.getSize()));
}

[[nodiscard]] PromptStateClipboardDecodeResult fail (juce::String message)
{
    PromptStateClipboardDecodeResult result;
    result.error = std::move (message);
    return result;
}

[[nodiscard]] juce::var getRequiredProperty (
    juce::DynamicObject& object,
    const juce::Identifier& key)
{
    if (! object.hasProperty (key))
        return {};
    return object.getProperty (key);
}

[[nodiscard]] juce::String normalizeClipboardText (juce::String text)
{
    text = text.trim();

    if (text.length() >= 2)
    {
        const auto first = text[0];
        const auto last = text[text.length() - 1];
        if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
            text = text.substring (1, text.length() - 1).trim();
    }

    return text;
}

} // namespace

juce::String encodePromptStateClipboardPayload (const PromptStateClipboardPayload& payload)
{
    auto root = juce::DynamicObject::Ptr (new juce::DynamicObject());
    root->setProperty ("version", 3);

    juce::Array<juce::var> prompts;
    for (const auto& prompt : payload.prompts)
    {
        auto promptObject = juce::DynamicObject::Ptr (new juce::DynamicObject());
        promptObject->setProperty ("text", prompt.text);
        promptObject->setProperty ("weightPercent", prompt.weightPercent);
        prompts.add (juce::var (promptObject.get()));
    }
    root->setProperty ("prompts", prompts);

    juce::Array<juce::var> styleCards;
    for (const auto& card : payload.styleCards)
    {
        auto cardObject = juce::DynamicObject::Ptr (new juce::DynamicObject());
        cardObject->setProperty ("id", card.id);
        cardObject->setProperty ("text", card.text);
        cardObject->setProperty ("weightPercent", card.weightPercent);
        cardObject->setProperty ("active", card.active);
        cardObject->setProperty ("colourArgb", static_cast<double> (card.colourArgb));
        cardObject->setProperty ("imageKey", card.imageKey);
        cardObject->setProperty ("userCreated", card.userCreated);
        if (card.bankColumn.has_value() && card.bankRow.has_value())
        {
            cardObject->setProperty ("bankColumn", *card.bankColumn);
            cardObject->setProperty ("bankRow", *card.bankRow);
        }
        styleCards.add (juce::var (cardObject.get()));
    }
    root->setProperty ("styleCards", styleCards);

    auto settings = juce::DynamicObject::Ptr (new juce::DynamicObject());
    settings->setProperty ("seed", payload.settings.seed);
    settings->setProperty ("temperature", payload.settings.temperature);
    settings->setProperty ("topK", payload.settings.topK);
    settings->setProperty ("guidanceWeight", payload.settings.guidanceWeight);
    settings->setProperty ("prebufferChunks", payload.settings.prebufferChunks);
    settings->setProperty ("maxQueueChunks", payload.settings.maxQueueChunks);
    settings->setProperty ("transitionDelaySeconds", payload.settings.transitionDelaySeconds);
    root->setProperty ("settings", juce::var (settings.get()));

    const auto json = juce::JSON::toString (
        juce::var (root.get()),
        juce::JSON::FormatOptions().withSpacing (juce::JSON::Spacing::none));
    return juce::Base64::toBase64 (json);
}

PromptStateClipboardDecodeResult decodePromptStateClipboardPayload (const juce::String& encoded)
{
    juce::MemoryBlock decodedBlock;
    juce::MemoryOutputStream decodedStream (decodedBlock, false);
    if (! juce::Base64::convertFromBase64 (decodedStream, normalizeClipboardText (encoded)))
        return fail ("Prompt state is not valid base64.");

    const auto json = memoryBlockToString (decodedBlock);
    const auto rootVar = juce::JSON::parse (json);
    auto* root = rootVar.getDynamicObject();
    if (root == nullptr)
        return fail ("Prompt state is not valid JSON.");

    const int version = static_cast<int> (getRequiredProperty (*root, "version"));
    if (version != 3)
        return fail ("Prompt state version is not supported.");

    const auto promptsVar = getRequiredProperty (*root, "prompts");
    auto* prompts = promptsVar.getArray();
    if (prompts == nullptr || prompts->size() != 4)
        return fail ("Prompt state must contain exactly four prompts.");

    const auto settingsVar = getRequiredProperty (*root, "settings");
    auto* settings = settingsVar.getDynamicObject();
    if (settings == nullptr)
        return fail ("Prompt state settings are missing.");

    PromptStateClipboardDecodeResult result;
    result.ok = true;
    result.payload.version = version;

    for (int i = 0; i < prompts->size(); ++i)
    {
        auto* prompt = prompts->getReference (i).getDynamicObject();
        if (prompt == nullptr)
            return fail ("Prompt state contains an invalid prompt slot.");

        result.payload.prompts[static_cast<std::size_t> (i)].text =
            getRequiredProperty (*prompt, "text").toString();
        result.payload.prompts[static_cast<std::size_t> (i)].weightPercent =
            static_cast<double> (getRequiredProperty (*prompt, "weightPercent"));
    }

    result.payload.settings.seed = getRequiredProperty (*settings, "seed").toString();
    result.payload.settings.temperature =
        static_cast<double> (getRequiredProperty (*settings, "temperature"));
    result.payload.settings.topK =
        static_cast<int> (getRequiredProperty (*settings, "topK"));
    result.payload.settings.guidanceWeight =
        static_cast<double> (getRequiredProperty (*settings, "guidanceWeight"));
    result.payload.settings.prebufferChunks =
        static_cast<int> (getRequiredProperty (*settings, "prebufferChunks"));
    result.payload.settings.maxQueueChunks =
        static_cast<int> (getRequiredProperty (*settings, "maxQueueChunks"));
    if (settings->hasProperty ("transitionDelaySeconds"))
    {
        result.payload.settings.transitionDelaySeconds =
            static_cast<double> (getRequiredProperty (*settings, "transitionDelaySeconds"));
    }

    const auto styleCardsVar = getRequiredProperty (*root, "styleCards");
    auto* styleCards = styleCardsVar.getArray();
    if (styleCards == nullptr)
        return fail ("Prompt state style cards are invalid.");

    result.payload.styleCards.reserve (static_cast<std::size_t> (styleCards->size()));
    for (const auto& cardVar : *styleCards)
    {
        auto* card = cardVar.getDynamicObject();
        if (card == nullptr)
            return fail ("Prompt state contains an invalid style card.");

        PromptStateClipboardStyleCard styleCard;
        styleCard.id = getRequiredProperty (*card, "id").toString();
        styleCard.text = getRequiredProperty (*card, "text").toString();
        styleCard.weightPercent =
            static_cast<double> (getRequiredProperty (*card, "weightPercent"));
        styleCard.active = static_cast<bool> (getRequiredProperty (*card, "active"));
        styleCard.colourArgb =
            static_cast<std::uint32_t> (static_cast<double> (getRequiredProperty (*card, "colourArgb")));
        styleCard.imageKey = getRequiredProperty (*card, "imageKey").toString();
        styleCard.userCreated = static_cast<bool> (getRequiredProperty (*card, "userCreated"));
        if (card->hasProperty ("bankColumn") || card->hasProperty ("bankRow"))
        {
            if (! card->hasProperty ("bankColumn") || ! card->hasProperty ("bankRow"))
                return fail ("Prompt state style card bank position is incomplete.");
            styleCard.bankColumn = static_cast<int> (getRequiredProperty (*card, "bankColumn"));
            styleCard.bankRow = static_cast<int> (getRequiredProperty (*card, "bankRow"));
        }
        result.payload.styleCards.push_back (std::move (styleCard));
    }

    return result;
}

} // namespace mrt::plugin
