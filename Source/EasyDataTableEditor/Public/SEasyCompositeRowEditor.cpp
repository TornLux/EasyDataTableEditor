#include "SEasyCompositeRowEditor.h"
class UDataTable;

#define LOCTEXT_NAMESPACE "SEasyCompositeRowEditor"

SEasyCompositeRowEditor::SEasyCompositeRowEditor()
	: SEasyRowEditor()
{
}

SEasyCompositeRowEditor::~SEasyCompositeRowEditor()
{
}

void SEasyCompositeRowEditor::Construct(const FArguments& InArgs, UDataTable* Changed)
{
	ConstructInternal(Changed);
}

FReply SEasyCompositeRowEditor::OnAddClicked()
{
	return SEasyRowEditor::OnAddClicked();
}

FReply SEasyCompositeRowEditor::OnRemoveClicked()
{
	return SEasyRowEditor::OnRemoveClicked();
}

FReply SEasyCompositeRowEditor::OnMoveRowClicked(FEasyDataTableEditorUtils::ERowMoveDirection MoveDirection)
{
	return SEasyRowEditor::OnMoveRowClicked(MoveDirection);
}

bool SEasyCompositeRowEditor::IsMoveRowUpEnabled() const
{
	return false;
}

bool SEasyCompositeRowEditor::IsMoveRowDownEnabled() const
{
	return false;
}

bool SEasyCompositeRowEditor::IsAddRowEnabled() const
{
	return false;
}

bool SEasyCompositeRowEditor::IsRemoveRowEnabled() const
{
	return false;
}

EVisibility SEasyCompositeRowEditor::GetRenameVisibility() const
{
	return EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE