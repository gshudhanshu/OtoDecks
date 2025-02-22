/*
  ==============================================================================

	PlaylistComponent.cpp
	Created: 23 Feb 2023 3:03:50pm
	Author:  Shudhanshu Gunjal

  ==============================================================================
*/

#include <JuceHeader.h>
#include "PlaylistComponent.h"
#include <future>

//==============================================================================
PlaylistComponent::PlaylistComponent(
	DeckGUI* _deckGUI1,
	DeckGUI* _deckGUI2) : deckGUI1(_deckGUI1), deckGUI2(_deckGUI2)
{
	// Import svg button SVGs
	playSvg = Drawable::createFromImageData(BinaryData::play_solid_svg, BinaryData::play_solid_svgSize);
	deleteSvg = Drawable::createFromImageData(BinaryData::trash_solid_svg, BinaryData::trash_solid_svgSize);

	// Default playlist directory
	String dir = File::getCurrentWorkingDirectory().getChildFile("playlist.txt").getFullPathName();
	// Auto import default playlist
	autoImportDefaultPlaylist(dir);

	// Table header setup
	tableComponent.getHeader().addColumn("Load A", 1, 50, 50, 50);
	tableComponent.getHeader().addColumn("Load B", 2, 50, 50, 50);
	tableComponent.getHeader().addColumn("Track title", 3, 100);
	tableComponent.getHeader().addColumn("Format", 4, 100, 70, 100);
	tableComponent.getHeader().addColumn("Time", 5, 100, 70, 100);
	tableComponent.getHeader().addColumn("Delete", 6, 50, 50, 50);
	tableComponent.getHeader().setStretchToFitActive(true);

	tableComponent.setModel(this);

	tableComponent.autoSizeAllColumns();
	addAndMakeVisible(tableComponent);

	searchInput.setTextToShowWhenEmpty("Search...", juce::Colours::lightgrey);
	searchInput.setJustification(Justification::centred);

	// Make buttons visible
	addAndMakeVisible(searchInput);
	addAndMakeVisible(importTracksBtn);
	addAndMakeVisible(importPlaylistBtn);
	addAndMakeVisible(exportPlaylistBtn);
	addAndMakeVisible(clearPlaylistBtn);

	// Add button listeners
	importTracksBtn.addListener(this);
	importPlaylistBtn.addListener(this);
	exportPlaylistBtn.addListener(this);
	clearPlaylistBtn.addListener(this);
	searchInput.onTextChange = [this] { searchTrackInPlaylist(searchInput.getText()); };
}

PlaylistComponent::~PlaylistComponent()
{
	String dir = File::getCurrentWorkingDirectory().getChildFile("playlist.txt").getFullPathName();
	autoExportDefaultPlaylist(dir);
	tableComponent.setModel(nullptr);
}

void PlaylistComponent::paint(juce::Graphics& g)
{

	g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));   // clear the background
	g.setColour(juce::Colour(0xff1e253a));
	g.drawRect(getLocalBounds(), 1);   // draw an outline around the component

	g.setColour(juce::Colours::white);
	g.setFont(14.0f);
	g.drawText("Actions", getLocalBounds(),
		juce::Justification::topLeft, true);   // draw some placeholder text
}

void PlaylistComponent::resized()
{
	// Initialize flexbox
	juce::FlexBox playlistFlex;
	juce::FlexBox actions;

	// Playlist action buttons
	actions.flexDirection = juce::FlexBox::Direction::column;
	actions.items.add(juce::FlexItem(searchInput).withFlex(1).withMaxHeight(30));
	actions.items.add(juce::FlexItem(importTracksBtn).withFlex(1).withMaxHeight(30));
	actions.items.add(juce::FlexItem(importPlaylistBtn).withFlex(1).withMaxHeight(30));
	actions.items.add(juce::FlexItem(exportPlaylistBtn).withFlex(1).withMaxHeight(30));
	actions.items.add(juce::FlexItem(clearPlaylistBtn).withFlex(1).withMaxHeight(30));

	playlistFlex.items.add(juce::FlexItem(actions).withFlex(1));
	playlistFlex.items.add(juce::FlexItem(tableComponent).withFlex(4));

	tableComponent.autoSizeAllColumns();

	playlistFlex.performLayout(getLocalBounds().toFloat());
}


int PlaylistComponent::getNumRows()
{
	// If search input is not empty, return the size of the filtered playlist array
	if (!searchInput.isEmpty()) {
		return filteredPlaylistArr.size();
	}
	return playlistArr.size();
}

void PlaylistComponent::paintRowBackground(Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
	if (rowIsSelected)
	{
		g.fillAll(Colours::orange);
	}
	else
	{
		g.fillAll(Colour(0xff1e253a));
	}
}

void PlaylistComponent::paintCell(Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
	// Create a temporary array to store the playlist array
	Array <juce::File> tracksArr;
	if (!searchInput.isEmpty())
	{
		tracksArr = filteredPlaylistArr;
	}
	else
	{
		tracksArr = playlistArr;
	}

	g.setColour(Colours::white);

	if (columnId == 3)
	{
		g.drawText(tracksArr[rowNumber].getFileName(), 10, 0, width, height, Justification::centredLeft);
	}
	if (columnId == 4 || columnId == 5)
	{
		AudioFormatManager formatManager;
		formatManager.registerBasicFormats();
		ScopedPointer<AudioFormatReader> reader = formatManager.createReaderFor(tracksArr[rowNumber]);

		// Get the metadata of the mp3 file
		// if (reader) {
		//        for (String key : reader->metadataValues.getAllKeys()) {
		//        DBG("Key: " + key + " value: " + reader->metadataValues.getValue(key, "unknown"));
		//        // DBG("length "+ reader->metadataValues.getAllKeys().size());
		//        }
		//
		//    }

		if (columnId == 4 && reader)
		{
			//Juce is not able to read the metadata of the mp3 file so I am using 'file format' instead of 'artist'.
			g.drawText(reader->getFormatName(), 0, 0, width, height, Justification::centredLeft);
		}

		if (columnId == 5 && reader)
		{
			int seconds = reader->lengthInSamples / reader->sampleRate;
			String time = utils.convertSecTohhmmssFormat(seconds);
			g.drawText(time, 0, 0, width, height, Justification::centredLeft);
		}
	}
}

Component* PlaylistComponent::refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, Component* existingComponentToUpdate)
{
	// If the existingComponentToUpdate component is null then update the component
	if (existingComponentToUpdate == nullptr)
	{
		if (columnId == 1) {
			DrawableButton* loadDeck1Btn = new DrawableButton{ "LOAD_DECK_1", DrawableButton::ButtonStyle::ImageOnButtonBackground };
			loadDeck1Btn->setImages(playSvg.get());
			loadDeck1Btn->addListener(this);
			int index = playlistArrIndex[rowNumber];
			String id = String(index);
			loadDeck1Btn->setComponentID(id);
			existingComponentToUpdate = loadDeck1Btn;
		}

		else if (columnId == 2)
		{
			DrawableButton* loadDeck2Btn = new DrawableButton{ "LOAD_DECK_2", DrawableButton::ButtonStyle::ImageOnButtonBackground };
			loadDeck2Btn->setImages(playSvg.get());
			loadDeck2Btn->addListener(this);
			int index = playlistArrIndex[rowNumber];
			String id = String(index);
			loadDeck2Btn->setComponentID(id);
			existingComponentToUpdate = loadDeck2Btn;
		}

		else if (columnId == 6)
		{
			DrawableButton* deleteTrackBtn = new DrawableButton{ "DELETE_TRACK", DrawableButton::ButtonStyle::ImageOnButtonBackground };
			deleteTrackBtn->setImages(deleteSvg.get());
			deleteTrackBtn->addListener(this);
			int index = playlistArrIndex[rowNumber];
			String id = String(index);
			deleteTrackBtn->setComponentID(id);
			existingComponentToUpdate = deleteTrackBtn;
		}

		return existingComponentToUpdate;
	}

	// If search input is not empty, update the component id of the existing component
	if (searchInput.isEmpty()) {
		existingComponentToUpdate->setComponentID(String(rowNumber));
		return existingComponentToUpdate;
	}

	// Else remove the remove the existing component
	return nullptr;
}

void PlaylistComponent::deleteTrackFromPlaylist(int id)
{
	const auto callback = juce::ModalCallbackFunction::create([this, id](int result) {

		// result == 0 means you click Cancel
		if (result == 0)
		{
			return;
		}

		// result == 1 means you click OK
		if (result == 1)
		{
			if (searchInput.isEmpty())
			{
				playlistArr.remove(id);
			}
			else
			{
				DBG("id: " << id);
				DBG("id: " << playlistArr[id].getFileName());
				playlistArr.remove(id);
			}
			tableComponent.updateContent();
			searchTrackInPlaylist(searchInput.getText());

		}
	});

	AlertWindow::showOkCancelBox(MessageBoxIconType::QuestionIcon, "Delete Track", "Are you sure you want to delete this track?", "Yes", "No", {},
		callback);
}

void PlaylistComponent::importTracksToPlaylist()
{
	auto flags = FileBrowserComponent::canSelectMultipleItems;
	musicTracksChooser.launchAsync(flags, [this](const FileChooser& chooser)
	{
		auto file = chooser.getResults();
		playlistArr.addArray(file);
		tableComponent.updateContent();
		tableComponent.repaint();
		tableComponent.selectRow(0);
	});
}

void PlaylistComponent::exportTracksFromPlaylist()
{
	// Create a temp file to store the playlist
	auto fileToSave = File::createTempFile("export_playlist.txt");

	// Write the playlist to the temp file
	for (auto file : playlistArr) {
		fileToSave.appendText(file.getFullPathName() + "\n");
	}

	auto flags = FileBrowserComponent::saveMode;

	// Export the temp file to the user's computer
	exportPlaylist.launchAsync(flags, [this, fileToSave](const FileChooser& chooser)
		{
			auto result = chooser.getURLResult();
	auto name = result.isEmpty() ? String()
		: (result.isLocalFile() ? result.getLocalFile().getFullPathName()
			: result.toString(true));
	if (!result.isEmpty())
	{
		// Delete the existing file
		File fileToDelete(name);
		fileToDelete.deleteFile();

		// Write the new content to the existing file
		std::unique_ptr<InputStream>  wi(fileToSave.createInputStream());
		std::unique_ptr<OutputStream> wo(fileToDelete.createOutputStream());

		if (wi.get() != nullptr && wo.get() != nullptr)
		{
			auto numWritten = wo->writeFromInputStream(*wi, -1);
			jassertquiet(numWritten > 0);
			wo->flush();
		}
	}
		});
}


void PlaylistComponent::autoExportDefaultPlaylist(String path)
{
	auto file = juce::File::getCurrentWorkingDirectory().getChildFile("playlist.txt");
	FileOutputStream output(file);

	if (output.openedOk())
	{
		output.setPosition(0);  // default would append
		output.truncate();

		// Append the playlist to the playlist.txt
		for (auto file : playlistArr) {
			output.writeText(file.getFullPathName() + "\n", false, false, "");
		}
	}
}

void PlaylistComponent::importExportedPlaylist()
{
	auto flags = FileBrowserComponent::canSelectMultipleItems;
	importPlaylist.launchAsync(flags, [this](const FileChooser& chooser)
	{
		auto result = chooser.getURLResult();
		auto name = result.isEmpty() ? String()
						  : (result.isLocalFile() ? result.getLocalFile().getFullPathName()
						  : result.toString(true));

		if (!result.isEmpty())
		{
			autoImportDefaultPlaylist(result.getLocalFile().getFullPathName());
		}
	});
}

void PlaylistComponent::autoImportDefaultPlaylist(String path)
{
	File file(path);

	if (!file.existsAsFile())
		return;  // file doesn't exist

	juce::FileInputStream inputStream(file); // [2]

	if (!inputStream.openedOk())
		return;  // failed to open

	while (!inputStream.isExhausted()) // [3]
	{
		auto line = inputStream.readNextLine();
		playlistArr.add(File(line));
	}

	// Update the table component
	tableComponent.updateContent();
	tableComponent.repaint();
}

void PlaylistComponent::clearPlaylist()
{
	const auto callback = juce::ModalCallbackFunction::create([this](int result) {
		// result == 0 means you click Cancel
		if (result == 0)
		{
			return;
		}

		// result == 1 means you click OK
		if (result == 1)
		{
			playlistArr.clear();
			tableComponent.updateContent();
		}
	});

	AlertWindow::showOkCancelBox(MessageBoxIconType::QuestionIcon, "Clear playlist", "Are you sure you want to clear playlist?", "Yes", "No", {},
		callback);
}

void PlaylistComponent::searchTrackInPlaylist(String textString)
{

	// Clear the filtered playlist array
	filteredPlaylistArr.clear();

	// Iterate through the playlist array and add the matching tracks to the filtered playlist array
	for (int i = 0; i < playlistArr.size(); i++)
	{
		if (playlistArr[i].getFileNameWithoutExtension().containsIgnoreCase(textString))
		{
			filteredPlaylistArr.add(playlistArr[i]);
		}
	}

	// Update the playlistArrIndex to reflect the new indices of the filtered tracks
	playlistArrIndex.clear();
	for (int i = 0; i < filteredPlaylistArr.size(); i++)
	{
		int index = playlistArr.indexOf(filteredPlaylistArr[i]);
		if (index != -1)
		{
			playlistArrIndex.add(index);
		}
	}

	// Update the table to display the filtered playlist
	tableComponent.updateContent();
	tableComponent.repaint();
	tableComponent.selectRow(0);
}

void PlaylistComponent::buttonClicked(Button* button)
{
	int id;
	// Get the id of the button
	if (button->getComponentID() != "")
	{
		id = std::stoi(button->getComponentID().toStdString());
	}

	// Delete track from playlist
	if (button->getButtonText() == "DELETE_TRACK")
	{
		deleteTrackFromPlaylist(id);
	}

	else if (button->getButtonText() == "LOAD_DECK_1")
	{
		DBG("LOAD_DECK_1");
		deckGUI1->loadTrackToDeck(playlistArr[id]);

		//Active track button is not working properly 
		//button->setColour(TextButton::buttonColourId, Colour(252, 183, 67));
		//button->setTitle("Active");

		//if (activeTrack1 != nullptr)
		//{
		//	activeTrack1->setColour(TextButton::buttonColourId, findColour(ResizableWindow::backgroundColourId));
		//	activeTrack1->setTitle("");
		//	activeTrack1 = nullptr;
		//}
		//activeTrack1 = button;
	}

	else if (button->getButtonText() == "LOAD_DECK_2")
	{
		DBG("LOAD_DECK_2");
		deckGUI2->loadTrackToDeck(playlistArr[id]);

		//Active track button is not working properly 
		//button->setColour(TextButton::buttonColourId, Colour(74, 244, 210));
		//button->setTitle("Active");

		//if (activeTrack2 != nullptr)
		//{
		//	activeTrack2->setColour(TextButton::buttonColourId, findColour(ResizableWindow::backgroundColourId));
		//	activeTrack2->setTitle("");
		//	activeTrack2 = nullptr;
		//}
		//activeTrack2 = button;
	}

	else if (button == &importTracksBtn)
	{
		importTracksToPlaylist();
	}

	else if (button == &importPlaylistBtn)
	{
		importExportedPlaylist();
	}

	else if (button == &exportPlaylistBtn)
	{
		exportTracksFromPlaylist();
	}

	else if (button == &clearPlaylistBtn)
	{
		clearPlaylist();
	}
}