/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     John Gaunt (jgaunt@netscape.com)
 *     Aaron Leventhal (aaronl@netscape.com)
 *     Kyle Yuan (kyle.yuan@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// NOTE: alphabetically ordered
#include "nsContentCID.h"
#include "nsIAccessibleEditableText.h"
#include "nsIAccessibleEventListener.h"
#include "nsIClipboard.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMHTMLBRElement.h"
#include "nsIDOMHTMLDivElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMRange.h"
#include "nsIDOMText.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDeviceContext.h"
#include "nsIDocument.h"
#include "nsIEditor.h"
#include "nsIFontMetrics.h"
#include "nsIFrame.h"
#include "nsIPlaintextEditor.h"
#include "nsIRenderingContext.h"
#include "nsITextContent.h"
#include "nsIWidget.h"
#include "nsStyleStruct.h"
#include "nsTextAccessible.h"
#include "nsTextFragment.h"

#ifdef MOZ_ACCESSIBILITY_ATK

static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

PRBool nsAccessibleText::gSuppressedNotifySelectionChanged = PR_FALSE;

NS_IMPL_ISUPPORTS1(nsAccessibleText, nsIAccessibleText)

/**
  * nsAccessibleText implements the nsIAccessibleText interface for static text which mTextNode
  *   has nsITextContent interface
  */
nsAccessibleText::nsAccessibleText()
{
}

nsAccessibleText::~nsAccessibleText()
{
}

void nsAccessibleText::SetTextNode(nsIDOMNode *aNode)
{
  mTextNode = aNode;
}

/**
  * nsIAccessibleText helpers
  */
nsresult nsAccessibleText::GetSelections(nsISelectionController **aSelCon, nsISelection **aDomSel)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  mTextNode->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> shell;
  doc->GetShellAt(0, getter_AddRefs(shell));
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContent> content(do_QueryInterface(mTextNode));
  nsIFrame *frame = nsnull;
  shell->GetPrimaryFrameFor(content, &frame);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  // Get the selection and selection controller
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;
  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  frame->GetSelectionController(context, getter_AddRefs(selCon));
  if (selCon)
    selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));

  NS_ENSURE_TRUE(selCon && domSel, NS_ERROR_FAILURE);

  PRBool isSelectionCollapsed;
  domSel->GetIsCollapsed(&isSelectionCollapsed);
  // Don't perform any actions when the selection is not collapsed
  NS_ENSURE_TRUE(isSelectionCollapsed, NS_ERROR_FAILURE);

  if (aSelCon) {
    *aSelCon = selCon;
    NS_ADDREF(*aSelCon);
  }

  if (aDomSel) {
    *aDomSel = domSel;
    NS_ADDREF(*aDomSel);
  }

  return NS_OK;
}

nsresult nsAccessibleText::DOMPointToOffset(nsISupports *aClosure, nsIDOMNode* aNode, PRInt32 aNodeOffset, PRInt32* aResult)
{
  NS_ENSURE_ARG_POINTER(aNode && aResult);

  *aResult = aNodeOffset;

  nsCOMPtr<nsISupportsArray> domNodeArray(do_QueryInterface(aClosure));
  if (domNodeArray) {
    // Static text, calculate the offset from a given set of (text) node
    PRUint32 textLength, totalLength = 0;
    PRUint32 index, count;
    domNodeArray->Count(&count);
    for (index = 0; index < count; index++) {
      nsIDOMNode* domNode = (nsIDOMNode *)domNodeArray->ElementAt(index);
      if (aNode == domNode) {
        *aResult = aNodeOffset + totalLength;
        break;
      }
      nsCOMPtr<nsIDOMText> domText(do_QueryInterface(domNode));
      if (domText) {
        domText->GetLength(&textLength);
        totalLength += textLength;
      }
    }

    return NS_OK;
  }

  nsCOMPtr<nsIEditor> editor(do_QueryInterface(aClosure));
  if (editor) { // revised according to nsTextControlFrame::DOMPointToOffset
    // Editable text, calculate the offset from the editor
    nsCOMPtr<nsIDOMElement> rootElement;
    editor->GetRootElement(getter_AddRefs(rootElement));
    nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));

    NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMNodeList> nodeList;

    nsresult rv = rootNode->GetChildNodes(getter_AddRefs(nodeList));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);

    PRUint32 length = 0;
    rv = nodeList->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!length || aNodeOffset < 0)
      return NS_OK;

    PRInt32 i, textOffset = 0;
    PRInt32 lastIndex = (PRInt32)length - 1;

    for (i = 0; i < (PRInt32)length; i++) {
      if (rootNode == aNode && i == aNodeOffset) {
        *aResult = textOffset;
        return NS_OK;
      }

      nsCOMPtr<nsIDOMNode> item;
      rv = nodeList->Item(i, getter_AddRefs(item));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

      if (item == aNode) {
        *aResult = textOffset + aNodeOffset;
        return NS_OK;
      }

      nsCOMPtr<nsIDOMText> domText(do_QueryInterface(item));

      if (domText) {
        PRUint32 textLength = 0;

        rv = domText->GetLength(&textLength);
        NS_ENSURE_SUCCESS(rv, rv);

        textOffset += textLength;
      }
      else {
        // Must be a BR node. If it's not the last BR node
        // under the root, count it as a newline.
        if (i != lastIndex)
          ++textOffset;
      }
    }

    NS_ASSERTION((aNode == rootNode && aNodeOffset == (PRInt32)length),
                 "Invalide node offset!");

    *aResult = textOffset;
  }

  return NS_OK;
}

nsresult nsAccessibleText::OffsetToDOMPoint(nsISupports *aClosure, PRInt32 aOffset, nsIDOMNode** aResult, PRInt32* aPosition)
{
  NS_ENSURE_ARG_POINTER(aResult && aPosition);

  *aResult = nsnull;
  *aPosition = 0;

  nsCOMPtr<nsIEditor> editor(do_QueryInterface(aClosure));
  if (editor) { // revised according to nsTextControlFrame::OffsetToDOMPoint
    nsCOMPtr<nsIDOMElement> rootElement;
    editor->GetRootElement(getter_AddRefs(rootElement));
    nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));

    NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMNodeList> nodeList;

    nsresult rv = rootNode->GetChildNodes(getter_AddRefs(nodeList));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);

    PRUint32 length = 0;

    rv = nodeList->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!length || aOffset < 0) {
      *aPosition = 0;
      *aResult = rootNode;
      NS_ADDREF(*aResult);
      return NS_OK;
    }

    PRInt32 textOffset = 0;
    PRUint32 lastIndex = length - 1;

    for (PRUint32 i=0; i<length; i++) {
      nsCOMPtr<nsIDOMNode> item;
      rv = nodeList->Item(i, getter_AddRefs(item));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDOMText> domText(do_QueryInterface(item));

      if (domText) {
        PRUint32 textLength = 0;

        rv = domText->GetLength(&textLength);
        NS_ENSURE_SUCCESS(rv, rv);

        // Check if aOffset falls within this range.
        if (aOffset >= textOffset && aOffset <= textOffset+(PRInt32)textLength) {
          *aPosition = aOffset - textOffset;
          *aResult = item;
          NS_ADDREF(*aResult);
          return NS_OK;
        }

        textOffset += textLength;

        // If there aren't any more siblings after this text node,
        // return the point at the end of this text node!

        if (i == lastIndex) {
          *aPosition = textLength;
          *aResult = item;
          NS_ADDREF(*aResult);
          return NS_OK;
        }
      }
      else {
        // Must be a BR node, count it as a newline.

        if (aOffset == textOffset || i == lastIndex) {
          // We've found the correct position, or aOffset takes us
          // beyond the last child under rootNode, just return the point
          // under rootNode that is in front of this br.

          *aPosition = i;
          *aResult = rootNode;
          NS_ADDREF(*aResult);
          return NS_OK;
        }

        ++textOffset;
      }
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult nsAccessibleText::GetCurrectOffset(nsISupports *aClosure, nsISelection *aDomSel, PRInt32 *aOffset)
{
  nsCOMPtr<nsIDOMNode> focusNode;
  aDomSel->GetFocusNode(getter_AddRefs(focusNode));
  aDomSel->GetFocusOffset(aOffset);
  return DOMPointToOffset(aClosure, focusNode, *aOffset, aOffset);
}

/*
Gets the specified text.

aBoundaryType means:

ATK_TEXT_BOUNDARY_CHAR
  The character before/at/after the offset is returned.

ATK_TEXT_BOUNDARY_WORD_START
  The returned string is from the word start before/at/after the offset to the next word start.

ATK_TEXT_BOUNDARY_WORD_END
  The returned string is from the word end before/at/after the offset to the next work end.

ATK_TEXT_BOUNDARY_SENTENCE_START
  The returned string is from the sentence start before/at/after the offset to the next sentence start.

ATK_TEXT_BOUNDARY_SENTENCE_END
  The returned string is from the sentence end before/at/after the offset to the next sentence end.

ATK_TEXT_BOUNDARY_LINE_START
  The returned string is from the line start before/at/after the offset to the next line start.

ATK_TEXT_BOUNDARY_LINE_END
  The returned string is from the line end before/at/after the offset to the next line start.
*/
nsresult nsAccessibleText::GetTextHelperCore(EGetTextType aType, nsAccessibleTextBoundary aBoundaryType,
                                         PRInt32 aOffset, PRInt32 *aStartOffset, PRInt32 *aEndOffset,
                                         nsISelectionController *aSelCon, nsISelection *aDomSel,
                                         nsISupports *aClosure, nsAString &aText)
{
  PRInt32 rangeCount;
  nsCOMPtr<nsIDOMRange> range, oldRange;
  aDomSel->GetRangeCount(&rangeCount);

  if (rangeCount == 0) { // ever happen?
    SetCaretOffset(aOffset); // a new range will be added here
    rangeCount++;
  }
  aDomSel->GetRangeAt(rangeCount - 1, getter_AddRefs(range));
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  // backup the original selection range to restore the selection status later
  range->CloneRange(getter_AddRefs(oldRange));

  // Step1: move caret to an appropriate start position
  // Step2: move caret to end postion and select the text
  PRBool isStep1Forward, isStep2Forward;  // Moving directions for two steps
  switch (aType)
  {
  case eGetBefore:
    isStep1Forward = PR_FALSE;
    isStep2Forward = PR_FALSE;
    break;
  case eGetAt:
    isStep1Forward = PR_FALSE;
    isStep2Forward = PR_TRUE;
    break;
  case eGetAfter:
    isStep1Forward = PR_TRUE;
    isStep2Forward = PR_TRUE;
    break;
  default:
    return NS_ERROR_INVALID_ARG;
  }

  // The start/end focus node may be not our mTextNode
  nsCOMPtr<nsIDOMNode> startFocusNode, endFocusNode;
  switch (aBoundaryType)
  {
  case BOUNDARY_CHAR:
    if (aType == eGetAfter) { // We need the character next to current position
      aSelCon->CharacterMove(isStep1Forward, PR_FALSE);
      GetCurrectOffset(aClosure, aDomSel, aStartOffset);
    }
    aSelCon->CharacterMove(isStep2Forward, PR_TRUE);
    GetCurrectOffset(aClosure, aDomSel, aEndOffset);
    break;
  case BOUNDARY_WORD_START:
    {
    PRBool dontMove = PR_FALSE;
    // If we are at the word boundary, don't move the caret in the first step
    if (aOffset == 0)
      dontMove = PR_TRUE;
    else {
      PRUnichar prevChar;
      GetCharacterAtOffset(aOffset - 1, &prevChar);
      if (prevChar == ' ' || prevChar == '\t' || prevChar == '\n')
        dontMove = PR_TRUE;
    }
    if (!dontMove) {
      aSelCon->WordMove(isStep1Forward, PR_FALSE); // Move caret to previous/next word start boundary
      GetCurrectOffset(aClosure, aDomSel, aStartOffset);
    }
    aSelCon->WordMove(isStep2Forward, PR_TRUE);  // Select previous/next word
    GetCurrectOffset(aClosure, aDomSel, aEndOffset);
    }
    break;
  case BOUNDARY_LINE_START:
    if (aType != eGetAt) {
      // XXX, don't support yet
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    aSelCon->IntraLineMove(PR_FALSE, PR_FALSE);  // Move caret to the line start
    GetCurrectOffset(aClosure, aDomSel, aStartOffset);
    aSelCon->IntraLineMove(PR_TRUE, PR_TRUE);    // Move caret to the line end and select the whole line
    GetCurrectOffset(aClosure, aDomSel, aEndOffset);
    break;
  case BOUNDARY_WORD_END:
  case BOUNDARY_LINE_END:
  case BOUNDARY_SENTENCE_START:
  case BOUNDARY_SENTENCE_END:
  case BOUNDARY_ATTRIBUTE_RANGE:
    return NS_ERROR_NOT_IMPLEMENTED;
  default:
    return NS_ERROR_INVALID_ARG;
  }

  nsXPIDLString text;
  // Get text from selection
  nsresult rv = aDomSel->ToString(getter_Copies(text));
  aDomSel->RemoveAllRanges();
  // restore the original selection range
  aDomSel->AddRange(oldRange);
  NS_ENSURE_SUCCESS(rv, rv);

  aText = text;

  // Ensure aStartOffset <= aEndOffset
  if (*aStartOffset > *aEndOffset) {
    PRInt32 tmp = *aStartOffset;
    *aStartOffset = *aEndOffset;
    *aEndOffset = tmp;
  }

  return NS_OK;
}

nsresult nsAccessibleText::GetTextHelper(EGetTextType aType, nsAccessibleTextBoundary aBoundaryType,
                                         PRInt32 aOffset, PRInt32 *aStartOffset, PRInt32 *aEndOffset,
                                         nsISupports *aClosure, nsAString &aText)
{
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;

  nsresult rv = GetSelections(getter_AddRefs(selCon), getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  //backup old settings
  PRInt16 displaySelection;
  selCon->GetDisplaySelection(&displaySelection);
  PRBool caretEnable;
  selCon->GetCaretEnabled(&caretEnable);

  //turn off display and caret
  selCon->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);
  selCon->SetCaretEnabled(PR_FALSE);

  //turn off nsCaretAccessible::NotifySelectionChanged
  gSuppressedNotifySelectionChanged = PR_TRUE;

  PRInt32 caretOffset;
  if (NS_SUCCEEDED(GetCaretOffset(&caretOffset))) {
    if (caretOffset != aOffset)
      SetCaretOffset(aOffset);
  }

  *aStartOffset = *aEndOffset = aOffset;
  rv = GetTextHelperCore(aType, aBoundaryType, aOffset, aStartOffset, aEndOffset, selCon, domSel, aClosure, aText);

  //turn on nsCaretAccessible::NotifySelectionChanged
  gSuppressedNotifySelectionChanged = PR_FALSE;

  //restore old settings
  selCon->SetDisplaySelection(displaySelection);
  selCon->SetCaretEnabled(caretEnable);

  return rv;
}

/**
  * nsIAccessibleText impl.
  */
/*
 * Gets the offset position of the caret (cursor).
 */
NS_IMETHODIMP nsAccessibleText::GetCaretOffset(PRInt32 *aCaretOffset)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> focusNode;
  domSel->GetFocusNode(getter_AddRefs(focusNode));
  if (focusNode != mTextNode)
    return NS_ERROR_FAILURE;

  return domSel->GetFocusOffset(aCaretOffset);
}

/*
 * Sets the caret (cursor) position to the specified offset.
 */
NS_IMETHODIMP nsAccessibleText::SetCaretOffset(PRInt32 aCaretOffset)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));
  NS_ENSURE_TRUE(range, NS_ERROR_OUT_OF_MEMORY);

  range->SetStart(mTextNode, aCaretOffset);
  range->SetEnd(mTextNode, aCaretOffset);
  domSel->RemoveAllRanges();
  return domSel->AddRange(range);
}

/*
 * Gets the character count.
 */
NS_IMETHODIMP nsAccessibleText::GetCharacterCount(PRInt32 *aCharacterCount)
{
  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(mTextNode));
  if (!textContent)
    return NS_ERROR_FAILURE;

  return textContent->GetTextLength(aCharacterCount);
}

/*
 * Gets the number of selected regions.
 */
NS_IMETHODIMP nsAccessibleText::GetSelectionCount(PRInt32 *aSelectionCount)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isSelectionCollapsed;
  rv = domSel->GetIsCollapsed(&isSelectionCollapsed);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isSelectionCollapsed)
    *aSelectionCount = 0;

  rv = domSel->GetRangeCount(aSelectionCount);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/*
 * Gets the specified text.
 */
NS_IMETHODIMP nsAccessibleText::GetText(PRInt32 aStartOffset, PRInt32 aEndOffset, nsAString &aText)
{
  nsAutoString text;
  mTextNode->GetNodeValue(text);
  aText = Substring(text, aStartOffset, aEndOffset - aStartOffset);
  return NS_OK;
}

NS_IMETHODIMP nsAccessibleText::GetTextBeforeOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                    PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetBefore, aBoundaryType, aOffset, aStartOffset, aEndOffset, nsnull, aText);
}

NS_IMETHODIMP nsAccessibleText::GetTextAtOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetAt, aBoundaryType, aOffset, aStartOffset, aEndOffset, nsnull, aText);
}

NS_IMETHODIMP nsAccessibleText::GetTextAfterOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                   PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetAfter, aBoundaryType, aOffset, aStartOffset, aEndOffset, nsnull, aText);
}

/*
 * Gets the specified text.
 */
NS_IMETHODIMP nsAccessibleText::GetCharacterAtOffset(PRInt32 aOffset, PRUnichar *aCharacter)
{
  nsAutoString text;
  nsresult rv = GetText(aOffset, aOffset + 1, text);
  NS_ENSURE_SUCCESS(rv, rv);
  *aCharacter = text.First();
  return NS_OK;
}

NS_IMETHODIMP nsAccessibleText::GetAttributeRange(PRInt32 aOffset, PRInt32 *aRangeStartOffset, 
                                                  PRInt32 *aRangeEndOffset, nsISupports **aAttribute)
{
  // will do better job later
  *aRangeStartOffset = aOffset;
  GetCharacterCount(aRangeEndOffset);
  *aAttribute = nsnull;
  return NS_OK;
}

/*
 * Given an offset, the x, y, width, and height values are filled appropriately.
 */
NS_IMETHODIMP nsAccessibleText::GetCharacterExtents(PRInt32 aOffset,
              PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight,
              nsAccessibleCoordType aCoordType)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  mTextNode->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> shell;
  doc->GetShellAt(0, getter_AddRefs(shell));
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContent> content(do_QueryInterface(mTextNode));
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsIFrame *frame = nsnull;
  shell->GetPrimaryFrameFor(content, &frame);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  nsRect frameRect;
  if (NS_FAILED(frame->GetRect(frameRect))) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRenderingContext> rc;
  shell->CreateRenderingContext(frame, getter_AddRefs(rc));
  NS_ENSURE_TRUE(rc, NS_ERROR_FAILURE);

  const nsStyleFont *font;
  frame->GetStyleData(eStyleStruct_Font, (const nsStyleStruct *&)font);
  NS_ENSURE_TRUE(font, NS_ERROR_FAILURE);

  const nsStyleVisibility *visibility;
  frame->GetStyleData(eStyleStruct_Visibility,
      (const nsStyleStruct *&)visibility);
  nsCOMPtr<nsIAtom> langGroup;
  if (visibility && visibility->mLanguage) {
    visibility->mLanguage->GetLanguageGroup(getter_AddRefs(langGroup));
  }

  if (NS_FAILED(rc->SetFont(font->mFont, langGroup))) {
    return NS_ERROR_FAILURE;
  }

  nsIFontMetrics *fm;
  rc->GetFontMetrics(fm);
  NS_ENSURE_TRUE(fm, NS_ERROR_FAILURE);

  PRUnichar ch;
  if (NS_FAILED(GetCharacterAtOffset(aOffset, &ch))) {
    return NS_ERROR_FAILURE;
  }

  float t2p;
  if (NS_FAILED(context->GetTwipsToPixels(&t2p))) {
    return NS_ERROR_FAILURE;
  }

  //Getting width
  nscoord tmpWidth;
  if (NS_SUCCEEDED(rc->GetWidth(ch, tmpWidth))) {
    *aWidth = NSTwipsToIntPixels(tmpWidth, t2p);
  }

  //Getting height
  nscoord tmpHeight;
  if (NS_SUCCEEDED(fm->GetHeight(tmpHeight))) {
    *aHeight = NSTwipsToIntPixels(tmpHeight, t2p);
  }

  //Getting x and y
  PRInt32 tmpX, tmpY;
  tmpX = frameRect.x;
  tmpY = frameRect.y;

  //add the width of the string before current char
  nsAutoString beforeString;
  nscoord beforeWidth;
  if (NS_SUCCEEDED(GetText(0, aOffset, beforeString)) &&
      NS_SUCCEEDED(rc->GetWidth(beforeString, beforeWidth))) {
    tmpX += beforeWidth;
  }

  //find the topest frame, add the offset recursively
  nsRect tmpRect;
  nsIFrame *parentFrame = nsnull, *tmpFrame = frame;
  nsresult rv = tmpFrame->GetParent(&parentFrame);
  while (NS_SUCCEEDED(rv) && parentFrame) {
    if (NS_SUCCEEDED(parentFrame->GetRect(tmpRect))) {
      tmpX += tmpRect.x;
      tmpY += tmpRect.y;
    }
    tmpFrame = parentFrame;
    rv = tmpFrame->GetParent(&parentFrame);
  }

  tmpX = NSTwipsToIntPixels(tmpX, t2p);
  tmpY = NSTwipsToIntPixels(tmpY, t2p);

  //change to screen co-ord
  nsIWidget *frameWidget;
  if (NS_SUCCEEDED(tmpFrame->GetWindow(context, &frameWidget))) {
    nsRect oldRect(tmpX, tmpY, 0, 0), newRect;
    if (NS_SUCCEEDED(frameWidget->WidgetToScreen(oldRect, newRect))) {
      tmpX = newRect.x;
      tmpY = newRect.y;
    }
  }

  if (aCoordType == COORD_TYPE_WINDOW) {
    //co-ord type = window
    nsCOMPtr<nsIDOMDocumentView> docView(do_QueryInterface(doc));
    NS_ENSURE_TRUE(docView, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMAbstractView> abstractView;
    docView->GetDefaultView(getter_AddRefs(abstractView));
    NS_ENSURE_TRUE(abstractView, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMWindowInternal> windowInter(do_QueryInterface(abstractView));
    NS_ENSURE_TRUE(windowInter, NS_ERROR_FAILURE);

    PRInt32 screenX,screenY;
    if (NS_FAILED(windowInter->GetScreenX(&screenX)) ||
        NS_FAILED(windowInter->GetScreenY(&screenY))) {
      return NS_ERROR_FAILURE;
    }

    *aX = tmpX - screenX;
    *aY = tmpY - screenY;
  }
  else {
    //default: co-ord type = screen
    *aX = tmpX;
    *aY = tmpY;
  }

  return NS_OK;
}

/*
 * Gets the offset of the character located at coordinates x and y. x and y are interpreted as being relative to
 * the screen or this widget's window depending on coords.
 */
NS_IMETHODIMP nsAccessibleText::GetOffsetAtPoint(PRInt32 aX, PRInt32 aY, nsAccessibleCoordType aCoordType, PRInt32 *aOffset)
{
  // will do better job later
  *aOffset = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

/*
 * Gets the start and end offset of the specified selection.
 */
NS_IMETHODIMP nsAccessibleText::GetSelectionBounds(PRInt32 aSelectionNum, PRInt32 *aStartOffset, PRInt32 *aEndOffset)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);
  if (aSelectionNum < 0 || aSelectionNum >= rangeCount)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMRange> range;
  domSel->GetRangeAt(aSelectionNum, getter_AddRefs(range));
  range->GetStartOffset(aStartOffset);
  range->GetEndOffset(aEndOffset);
  return NS_OK;
}

/*
 * Changes the start and end offset of the specified selection.
 */
NS_IMETHODIMP nsAccessibleText::SetSelectionBounds(PRInt32 aSelectionNum, PRInt32 aStartOffset, PRInt32 aEndOffset)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);
  if (aSelectionNum < 0 || aSelectionNum >= rangeCount)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMRange> range;
  domSel->GetRangeAt(aSelectionNum, getter_AddRefs(range));

  nsCOMPtr<nsIDOMNode> startParent;
  nsCOMPtr<nsIDOMNode> endParent;
  range->GetStartContainer(getter_AddRefs(startParent));
  range->GetEndContainer(getter_AddRefs(endParent));
  PRInt32 oldEndOffset;
  range->GetEndOffset(&oldEndOffset);
  // to avoid set start point after the current end point
  if (aStartOffset < oldEndOffset) {
    range->SetStart(startParent, aStartOffset);
    range->SetEnd(endParent, aEndOffset);
  }
  else {
    range->SetEnd(endParent, aEndOffset);
    range->SetStart(startParent, aStartOffset);
  }
  return NS_OK;
}

/*
 * Adds a selection bounded by the specified offsets.
 */
NS_IMETHODIMP nsAccessibleText::AddSelection(PRInt32 aStartOffset, PRInt32 aEndOffset)
{
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;

  if (NS_FAILED(GetSelections(getter_AddRefs(selCon), getter_AddRefs(domSel))))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));
  if (! range)
    return NS_ERROR_OUT_OF_MEMORY;

  range->SetStart(mTextNode, aStartOffset);
  range->SetEnd(mTextNode, aEndOffset);
  return domSel->AddRange(range);
}

/*
 * Removes the specified selection.
 */
NS_IMETHODIMP nsAccessibleText::RemoveSelection(PRInt32 aSelectionNum)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);
  if (aSelectionNum < 0 || aSelectionNum >= rangeCount)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMRange> range;
  domSel->GetRangeAt(aSelectionNum, getter_AddRefs(range));
  return domSel->RemoveRange(range);
}

/**
  * nsAccessibleEditableText implements the nsIAccessibleText interface for editable text, such as HTML 
  *   <input>, <textarea> and XUL <editor>
  */
NS_IMPL_ISUPPORTS1(nsAccessibleEditableText, nsIAccessibleText)

nsAccessibleEditableText::nsAccessibleEditableText()
{
}

nsAccessibleEditableText::~nsAccessibleEditableText()
{
}

/**
  * nsIAccessibleText helpers
  */

void nsAccessibleEditableText::SetEditor(nsIEditor* aEditor)
{
  mEditor = aEditor;
}

PRBool nsAccessibleEditableText::IsSingleLineTextControl(nsIDOMNode *aDomNode)
{
  nsCOMPtr<nsIDOMHTMLInputElement> input(do_QueryInterface(aDomNode));
  return input ? PR_TRUE : PR_FALSE;
}

nsresult nsAccessibleEditableText::FireTextChangeEvent(AtkTextChange *aTextData)
{
  nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(NS_STATIC_CAST(nsIAccessibleText*, this)));
  if (accessible) {
    printf("  [start=%d, length=%d, add=%d]\n", aTextData->start, aTextData->length, aTextData->add);
    accessible->HandleEvent(nsIAccessibleEventListener::EVENT_ATK_TEXT_CHANGE, accessible, aTextData);
  }

  return NS_OK;
}

nsITextControlFrame* nsAccessibleEditableText::GetTextFrame()
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  mTextNode->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  NS_ENSURE_TRUE(doc, nsnull);

  nsCOMPtr<nsIPresShell> shell;
  doc->GetShellAt(0, getter_AddRefs(shell));
  NS_ENSURE_TRUE(shell, nsnull);

  nsCOMPtr<nsIContent> content(do_QueryInterface(mTextNode));
  nsIFrame *frame = nsnull;
  shell->GetPrimaryFrameFor(content, &frame);
  NS_ENSURE_TRUE(frame, nsnull);

  nsITextControlFrame *textFrame;
  frame->QueryInterface(NS_GET_IID(nsITextControlFrame), (void**)&textFrame);

  return textFrame;
}

nsresult nsAccessibleEditableText::GetSelectionRange(PRInt32 *aStartPos, PRInt32 *aEndPos)
{
  *aStartPos = 0;
  *aEndPos = 0;

  nsITextControlFrame *textFrame = GetTextFrame();
  if (textFrame) {
    return textFrame->GetSelectionRange(aStartPos, aEndPos);
  }
  else {
  }

  return NS_OK;
}

nsresult nsAccessibleEditableText::SetSelectionRange(PRInt32 aStartPos, PRInt32 aEndPos)
{
  nsITextControlFrame *textFrame = GetTextFrame();
  if (textFrame) {
    return textFrame->SetSelectionRange(aStartPos, aEndPos);
  }
  else {
  }

  return NS_OK;
}

/**
  * nsIAccessibleText impl.
  */
/*
 * Gets the offset position of the caret (cursor).
 */
NS_IMETHODIMP nsAccessibleEditableText::GetCaretOffset(PRInt32 *aCaretOffset)
{
  *aCaretOffset = 0;

  PRInt32 startPos, endPos;
  nsresult rv = GetSelectionRange(&startPos, &endPos);
  NS_ENSURE_SUCCESS(rv, rv);

  if (startPos == endPos) { // selection must be collapsed
    *aCaretOffset = startPos;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

/*
 * Sets the caret (cursor) position to the specified offset.
 */
NS_IMETHODIMP nsAccessibleEditableText::SetCaretOffset(PRInt32 aCaretOffset)
{
  return SetSelectionRange(aCaretOffset, aCaretOffset);
}

/*
 * Gets the character count.
 */
NS_IMETHODIMP nsAccessibleEditableText::GetCharacterCount(PRInt32 *aCharacterCount)
{
  *aCharacterCount = 0;

  nsITextControlFrame *textFrame = GetTextFrame();
  if (textFrame) {
    textFrame->GetTextLength(aCharacterCount);
    return NS_OK;
  }
  else {
  }

  return NS_ERROR_FAILURE;
}

/*
 * Gets the specified text.
 */
NS_IMETHODIMP nsAccessibleEditableText::GetText(PRInt32 aStartOffset, PRInt32 aEndOffset, nsAString & aText)
{
  nsITextControlFrame *textFrame = GetTextFrame();
  if (textFrame) {
    nsAutoString text;
    textFrame->GetValue(text, PR_TRUE);

    if (aEndOffset == -1) // get all text from aStartOffset
      aEndOffset = text.Length();

    aText = Substring(text, aStartOffset, aEndOffset - aStartOffset);
    return NS_OK;
  }
  else {
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAccessibleEditableText::GetTextBeforeOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                    PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetBefore, aBoundaryType, aOffset, aStartOffset, aEndOffset, mEditor, aText);
}

NS_IMETHODIMP nsAccessibleEditableText::GetTextAtOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetAt, aBoundaryType, aOffset, aStartOffset, aEndOffset, mEditor, aText);
}

NS_IMETHODIMP nsAccessibleEditableText::GetTextAfterOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                   PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetAfter, aBoundaryType, aOffset, aStartOffset, aEndOffset, mEditor, aText);
}

#endif //MOZ_ACCESSIBILITY_ATK

// ------------
// Text Accessibles
// ------------

nsTextAccessible::nsTextAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsLinkableAccessible(aDOMNode, aShell)
{ 
#ifdef MOZ_ACCESSIBILITY_ATK
  SetTextNode(aDOMNode);
#endif
}

#ifndef MOZ_ACCESSIBILITY_ATK
NS_IMPL_ISUPPORTS_INHERITED0(nsTextAccessible, nsLinkableAccessible)
#else
NS_IMPL_ISUPPORTS_INHERITED1(nsTextAccessible, nsLinkableAccessible, nsAccessibleText)
#endif

/**
  * We are text
  */
NS_IMETHODIMP nsTextAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_TEXT;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsTextAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsTextAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsTextAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}
