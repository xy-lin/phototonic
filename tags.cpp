/*
 *  Copyright (C) 2013-2015 Ofer Kashayov <oferkv@live.com>
 *  This file is part of Phototonic Image Viewer.
 *
 *  Phototonic is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Phototonic is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Phototonic.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tags.h"
#include "global.h"
#include "dialogs.h"

ImageTags::ImageTags(QWidget *parent, ThumbView *thumbView, MetadataCache *mdCache) : QWidget(parent)
{
	tagsTree = new QTreeWidget;
	tagsTree->setColumnCount(2);
	tagsTree->setDragEnabled(false);
	tagsTree->setSortingEnabled(true);
	tagsTree->header()->close();
	tagsTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	this->thumbView = thumbView;
	this->mdCache = mdCache;
	negateFilterEnabled = false;

	tabs = new QTabBar(this);
	tabs->addTab(tr("Image Tags"));
  	tabs->addTab(tr("Filter"));
  	tabs->setTabIcon(0, QIcon(":/images/tag_yellow.png"));
  	tabs->setTabIcon(1, QIcon(":/images/tag_filter_off.png"));
	connect(tabs, SIGNAL(currentChanged(int)), this, SLOT(tabsChanged(int)));

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(0, 3, 0, 0);
	mainLayout->setSpacing(0);
	mainLayout->addWidget(tabs);
	mainLayout->addWidget(tagsTree);
	setLayout(mainLayout);
	currentDisplayMode = SelectionTagsDisplay;
	folderFilteringActive = false;

	connect(tagsTree, SIGNAL(itemChanged(QTreeWidgetItem *, int)),
				this, SLOT(saveLastChangedTag(QTreeWidgetItem *, int)));
	connect(tagsTree, SIGNAL(itemClicked(QTreeWidgetItem *, int)),
				this, SLOT(tagClicked(QTreeWidgetItem *, int)));

	tagsTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(tagsTree, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showMenu(QPoint)));

	addToSelectionAction = new QAction(tr("Tag"), this);
	addToSelectionAction->setIcon(QIcon(":/images/tag_yellow.png"));
	connect(addToSelectionAction, SIGNAL(triggered()), this, SLOT(addTagsToSelection()));

	removeFromSelectionAction = new QAction(tr("Untag"), this);
	connect(removeFromSelectionAction, SIGNAL(triggered()), this, SLOT(removeTagsFromSelection()));

	addTagAction = new QAction(tr("New Tag"), this);
	addTagAction->setIcon(QIcon(":/images/new_tag.png"));
	connect(addTagAction, SIGNAL(triggered()), this, SLOT(addNewTag()));

	removeTagAction = new QAction(tr("Remove Tag"), this);
	removeTagAction->setIcon(QIcon::fromTheme("edit-delete", QIcon(":/images/delete.png")));

	clearTagsFilterAction = new QAction(tr("Clear Filters"), this);
	clearTagsFilterAction->setIcon(QIcon(":/images/tag_filter_off.png"));
	connect(clearTagsFilterAction, SIGNAL(triggered()), this, SLOT(clearTagFilters()));

	negateAction = new QAction(tr("Negate"), this);
	negateAction->setCheckable(true);
	connect(negateAction, SIGNAL(triggered()), this, SLOT(negateFilter()));

	tagsMenu = new QMenu("");
	tagsMenu->addAction(addToSelectionAction);
	tagsMenu->addAction(removeFromSelectionAction);
	tagsMenu->addSeparator();
	tagsMenu->addAction(addTagAction);
	tagsMenu->addAction(removeTagAction);
	tagsMenu->addSeparator();
	tagsMenu->addAction(clearTagsFilterAction);
	tagsMenu->addAction(negateAction);
}

void ImageTags::redrawTree()
{
	tagsTree->resizeColumnToContents(0);
	tagsTree->sortItems(0, Qt::AscendingOrder);
}

void ImageTags::showMenu(QPoint pt)
{
    QTreeWidgetItem *item = tagsTree->itemAt(pt);
	addToSelectionAction->setEnabled(item? true : false);
	removeFromSelectionAction->setEnabled(item? true : false);
	removeTagAction->setEnabled(item? true : false);
    tagsMenu->popup(tagsTree->viewport()->mapToGlobal(pt));
}

void ImageTags::setTagIcon(QTreeWidgetItem *tagItem, TagIcons icon)
{
	switch (icon) {
		case TagIconDisabled:
		  	tagItem->setIcon(0, QIcon(":/images/tag_grey.png"));
		  	break;
		case TagIconEnabled:
		  	tagItem->setIcon(0, QIcon(":/images/tag_yellow.png"));
		  	break;
		case TagIconMultiple:
			tagItem->setIcon(0, QIcon(":/images/tag_multi.png"));
		  	break;
		case TagIconFilterEnabled:
			tagItem->setIcon(0, QIcon(":/images/tag_filter_on.png"));
		  	break;
		case TagIconFilterDisabled:
			tagItem->setIcon(0, QIcon(":/images/tag_filter_off.png"));
		  	break;
		case TagIconFilterNegate:
			tagItem->setIcon(0, QIcon(":/images/tag_filter_negate.png"));
		  	break;
	}
}

void ImageTags::addTag(QString tagName, bool tagChecked)
{
	QTreeWidgetItem *tagItem = new QTreeWidgetItem();

    tagItem->setText(0, tagName);
	tagItem->setCheckState(0, tagChecked? Qt::Checked : Qt::Unchecked);
	setTagIcon(tagItem, tagChecked? TagIconEnabled : TagIconDisabled);
	tagsTree->addTopLevelItem(tagItem);
}

bool ImageTags::writeTagsToImage(QString &imageFileName, QSet<QString> &newTags)
{
	QSet<QString> imageTags;
	Exiv2::Image::AutoPtr exifImage;
	static Exiv2::Value::AutoPtr characterSet = Exiv2::Value::create(Exiv2::string);

	characterSet->read("\x1b%G");
	
	try {
		exifImage = Exiv2::ImageFactory::open(imageFileName.toStdWString());
		exifImage->readMetadata();

		Exiv2::IptcData newIptcData;

		/* copy existing data */
		Exiv2::IptcData &iptcData = exifImage->iptcData();
		
		if (!iptcData.empty()) 
		{
			QString key;
			Exiv2::IptcData::iterator end = iptcData.end();

			for (Exiv2::IptcData::iterator iptcIt = iptcData.begin(); iptcIt != end; ++iptcIt) 
			{
				if (iptcIt->tagName() != "Keywords" && iptcIt->tagName() != "CharacterSet")
				{
					newIptcData.add(*iptcIt);
				}				
			}
		}

		Exiv2::IptcKey key("Iptc.Envelope.CharacterSet");

		newIptcData.add(key, characterSet.get());	


		/* add new tags */
		QSetIterator<QString> newTagsIt(newTags);

		while (newTagsIt.hasNext()) 
		{
			QString tag = newTagsIt.next();
			Exiv2::Value::AutoPtr value = Exiv2::Value::create(Exiv2::string);

			value->read(tag.toStdString());
			
			Exiv2::IptcKey key("Iptc.Application2.Keywords");
			
			newIptcData.add(key, value.get());
		}

		exifImage->setIptcData(newIptcData);
		exifImage->writeMetadata();
	}
	catch (Exiv2::Error &error) 
	{
		QMessageBox msgBox;
		msgBox.critical(this, tr("Error"), tr("Failed to save tags to ") + imageFileName);
		return false;
	}

	return true;
}

void ImageTags::showSelectedImagesTags()
{
	static bool busy = false;
	if (busy)
		return;
	busy = true;
	QStringList selectedThumbs = thumbView->getSelectedThumbsList();

	setActiveViewMode(SelectionTagsDisplay);

	int selectedThumbsNum = selectedThumbs.size();
	QMap<QString, int> tagsCount;
	for (int i = 0; i < selectedThumbsNum; ++i) {
		QSetIterator<QString> imageTagsIter(mdCache->getImageTags(selectedThumbs[i]));
		while (imageTagsIter.hasNext()) {
			QString imageTag = imageTagsIter.next();
			tagsCount[imageTag]++;

			if (!GData::knownTags.contains(imageTag)) {
				addTag(imageTag, true);
				GData::knownTags.insert(imageTag);
			}
		}
	}

	bool imagesTagged = false, imagesTaggedMixed = false;
	QTreeWidgetItemIterator it(tagsTree);
    while (*it) {
    	QString tagName = (*it)->text(0);
    	int tagCountTotal = tagsCount[tagName];

        if (selectedThumbsNum == 0) {
        	(*it)->setCheckState(0, Qt::Unchecked);
        	(*it)->setFlags((*it)->flags() & ~Qt::ItemIsUserCheckable);
        	setTagIcon(*it, TagIconDisabled);
		} else if (tagCountTotal ==  selectedThumbsNum) {
        	(*it)->setCheckState(0, Qt::Checked);
			(*it)->setFlags((*it)->flags() | Qt::ItemIsUserCheckable);
        	setTagIcon(*it, TagIconEnabled);
        	imagesTagged = true;
       	} else if (tagCountTotal) {
			(*it)->setCheckState(0, Qt::PartiallyChecked);
			(*it)->setFlags((*it)->flags() | Qt::ItemIsUserCheckable);
        	setTagIcon(*it, TagIconMultiple);
        	imagesTaggedMixed = true;
		} else {
        	(*it)->setCheckState(0, Qt::Unchecked);
			(*it)->setFlags((*it)->flags() | Qt::ItemIsUserCheckable);
        	setTagIcon(*it, TagIconDisabled);
		}
        ++it;
    }

	if (imagesTagged) {
	   	tabs->setTabIcon(0, QIcon(":/images/tag_yellow.png"));	
	} else if (imagesTaggedMixed) {
	   	tabs->setTabIcon(0, QIcon(":/images/tag_multi.png"));	
	} else {
	   	tabs->setTabIcon(0, QIcon(":/images/tag_grey.png"));	
	}

	addToSelectionAction->setEnabled(selectedThumbsNum? true : false);
	removeFromSelectionAction->setEnabled(selectedThumbsNum? true : false);

	redrawTree();
   	busy = false;
}

void ImageTags::showTagsFilter()
{
	static bool busy = false;
	if (busy)
		return;
	busy = true;

	setActiveViewMode(FolderTagsDisplay);

	QTreeWidgetItemIterator it(tagsTree);
    while (*it) {
    	QString tagName = (*it)->text(0);

		(*it)->setFlags((*it)->flags() | Qt::ItemIsUserCheckable);
        if (imageFilteringTags.contains(tagName)) {
        	(*it)->setCheckState(0, Qt::Checked);
        	setTagIcon(*it, negateFilterEnabled? TagIconFilterNegate : TagIconFilterEnabled);
		} else {
        	(*it)->setCheckState(0, Qt::Unchecked);
        	setTagIcon(*it, TagIconFilterDisabled);
       	} 
        ++it;
    }

	redrawTree();
	busy = false;
}

void ImageTags::populateTagsTree()
{
	tagsTree->clear();
	QSetIterator<QString> knownTagsIt(GData::knownTags);
	while (knownTagsIt.hasNext()) {
	    QString tag = knownTagsIt.next();
        addTag(tag, false);
    }

	redrawTree();
}

void ImageTags::setActiveViewMode(TagsDisplayMode mode)
{
	currentDisplayMode = mode;
	addTagAction->setVisible(currentDisplayMode == SelectionTagsDisplay);
	removeTagAction->setVisible(currentDisplayMode == SelectionTagsDisplay);
	addToSelectionAction->setVisible(currentDisplayMode == SelectionTagsDisplay);
	removeFromSelectionAction->setVisible(currentDisplayMode == SelectionTagsDisplay);
	clearTagsFilterAction->setVisible(currentDisplayMode == FolderTagsDisplay);
	negateAction->setVisible(currentDisplayMode == FolderTagsDisplay);
}

bool ImageTags::isImageFilteredOut(QString imageFileName)
{
	QSet<QString> imageTags = mdCache->getImageTags(imageFileName);

	QSetIterator<QString> filteredTagsIt(imageFilteringTags);
	while (filteredTagsIt.hasNext()) {
	    if (imageTags.contains(filteredTagsIt.next())) {
			return negateFilterEnabled? true : false;
		}
	}

	return negateFilterEnabled? false : true;
}

void ImageTags::resetTagsState()
{
	tagsTree->clear();
	mdCache->clear();
}

QSet<QString> ImageTags::getCheckedTags(Qt::CheckState tagState)
{
	QSet<QString> checkedTags;
    QTreeWidgetItemIterator it(tagsTree);

    while (*it) {
        if ((*it)->checkState(0) == tagState) {
	        checkedTags.insert((*it)->text(0));
        }
        ++it;
    }

	return checkedTags;
}

void ImageTags::applyTagFiltering()
{
	imageFilteringTags = getCheckedTags(Qt::Checked);
	if (imageFilteringTags.size()) {
		folderFilteringActive = true;
		if (negateFilterEnabled) {
		 	tabs->setTabIcon(1, QIcon(":/images/tag_filter_negate.png"));
	 	} else {
		 	tabs->setTabIcon(1, QIcon(":/images/tag_filter_on.png"));
	 	}
	} else {
		folderFilteringActive = false;
	 	tabs->setTabIcon(1, QIcon(":/images/tag_filter_off.png"));
	}
	
	emit reloadThumbs();
}

void ImageTags::applyUserAction(QTreeWidgetItem *item)
{
	QList<QTreeWidgetItem *> tagsList;
	tagsList << item;
	applyUserAction(tagsList);
}

void ImageTags::applyUserAction(QList<QTreeWidgetItem *> tagsList)
{
	int processEventsCounter = 0;
	ProgressDialog *dialog = new ProgressDialog(this);
	dialog->show();

	QStringList currentSelectedImages = thumbView->getSelectedThumbsList();
	for (int i = 0; i < currentSelectedImages.size(); ++i) {

		QString imageName = currentSelectedImages[i]; 
		for (int i = tagsList.size() - 1; i > -1; --i) {
			Qt::CheckState tagState = tagsList.at(i)->checkState(0);
			setTagIcon(tagsList.at(i), (tagState == Qt::Checked? TagIconEnabled : TagIconDisabled));
			QString tagName = tagsList.at(i)->text(0);

			if (tagState == Qt::Checked) {
				dialog->opLabel->setText(tr("Tagging ") + imageName);
				mdCache->addTagToImage(imageName, tagName);
			} else {
				dialog->opLabel->setText(tr("Untagging ") + imageName);
				mdCache->removeTagFromImage(imageName, tagName);
			}
		}

		if (!writeTagsToImage(imageName, mdCache->getImageTags(imageName))) 
		{
			mdCache->removeImage(imageName);
		}

		++processEventsCounter;
		if (processEventsCounter > 9) {
			processEventsCounter = 0;
			QApplication::processEvents();
		}

		if (dialog->abortOp) {
			break;
		}
	}

	dialog->close();
	delete(dialog);
}

void ImageTags::saveLastChangedTag(QTreeWidgetItem *item, int)
{
	lastChangedTagItem = item;
}

void ImageTags::tabsChanged(int index)
{
	if (!index) {
		showSelectedImagesTags();
	} else {
		showTagsFilter();
	}
}

void ImageTags::tagClicked(QTreeWidgetItem *item, int)
{
	if (item == lastChangedTagItem) {
		if (currentDisplayMode == FolderTagsDisplay) {
			applyTagFiltering();
		} else {
			applyUserAction(item);
		}
		lastChangedTagItem = 0;
	}
}

void ImageTags::removeTagsFromSelection()
{	
	for (int i = tagsTree->selectedItems().size() - 1; i > -1; --i) {
		tagsTree->selectedItems().at(i)->setCheckState(0, Qt::Unchecked);
	}

	applyUserAction(tagsTree->selectedItems());
}

void ImageTags::addTagsToSelection()
{	
	for (int i = tagsTree->selectedItems().size() - 1; i > -1; --i) {
		tagsTree->selectedItems().at(i)->setCheckState(0, Qt::Checked);
	}

	applyUserAction(tagsTree->selectedItems());
}

void ImageTags::clearTagFilters()
{	
    QTreeWidgetItemIterator it(tagsTree);
    while (*it) {
        (*it)->setCheckState(0, Qt::Unchecked);
        ++it;
    }

	imageFilteringTags.clear();
	applyTagFiltering();
}

void ImageTags::negateFilter()
{	
	negateFilterEnabled = negateAction->isChecked();
	applyTagFiltering();
}

void ImageTags::addNewTag()
{	
	bool ok;
	QString title = tr("Add a new tag");
	QString newTagName = QInputDialog::getText(this, title, tr("Enter new tag name"),
												QLineEdit::Normal, "", &ok);
	if (!ok) {
		return;
	}

	if(newTagName.isEmpty()) {
		QMessageBox msgBox;
		msgBox.critical(this, tr("Error"), tr("No name entered"));
		return;
	}

	QSetIterator<QString> knownTagsIt(GData::knownTags);
	while (knownTagsIt.hasNext()) {
	    QString tag = knownTagsIt.next();
	    if (newTagName == tag) {
			QMessageBox msgBox;
			msgBox.critical(this, tr("Error"), tr("Tag ") + newTagName + tr(" already exists"));
			return;
        }
    }
	addTag(newTagName, false);
	GData::knownTags.insert(newTagName);
	redrawTree();
}

void ImageTags::removeTag()
{	
	if (!tagsTree->selectedItems().size()) {
		return;
	}

	QMessageBox msgBox;
	msgBox.setText(tr("Remove selected tags(s)?"));
	msgBox.setWindowTitle(tr("Remove tag"));
	msgBox.setIcon(QMessageBox::Warning);
	msgBox.setWindowIcon(QIcon(":/images/tag.png"));
	msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
	msgBox.setDefaultButton(QMessageBox::Cancel);
	msgBox.setButtonText(QMessageBox::Yes, tr("Yes"));  
    msgBox.setButtonText(QMessageBox::Cancel, tr("Cancel"));  

	if (msgBox.exec() != QMessageBox::Yes) {
		return;
	}

	bool removedTagWasChecked = false;
	for (int i = tagsTree->selectedItems().size() - 1; i > -1; --i) {

		QString tagName = tagsTree->selectedItems().at(i)->text(0);
		GData::knownTags.remove(tagName);

		if (imageFilteringTags.contains(tagName)) {
			imageFilteringTags.remove(tagName);      
			removedTagWasChecked = true;
		}

		tagsTree->takeTopLevelItem(tagsTree->indexOfTopLevelItem(tagsTree->selectedItems().at(i)));
	}

	if (removedTagWasChecked) {
		applyTagFiltering();
	}
}

