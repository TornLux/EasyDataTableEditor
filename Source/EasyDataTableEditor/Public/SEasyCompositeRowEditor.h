#pragma once

#include "DataTableEditorUtils.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "SEasyRowEditor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UDataTable;


class SEasyCompositeRowEditor : public SEasyRowEditor
{
	SLATE_BEGIN_ARGS(SEasyRowEditor) {}
	SLATE_END_ARGS()

	SEasyCompositeRowEditor();
	virtual ~SEasyCompositeRowEditor();

	void Construct(const FArguments& InArgs, UDataTable* Changed);

protected:
	virtual FReply OnAddClicked() override;
	virtual FReply OnRemoveClicked() override;
	virtual FReply OnMoveRowClicked(FEasyDataTableEditorUtils::ERowMoveDirection MoveDirection) override;

	virtual bool IsMoveRowUpEnabled() const override;
	virtual bool IsMoveRowDownEnabled() const override;
	virtual bool IsAddRowEnabled() const override;
	virtual bool IsRemoveRowEnabled() const override;
	virtual EVisibility GetRenameVisibility() const override;
};
