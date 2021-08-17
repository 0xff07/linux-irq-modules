#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/interrupt.h>

struct arduino_irq_dev {
	struct platform_device *pdev;
	int irq;
	struct tasklet_struct t;
};

static irqreturn_t arduino_irq_top_half(int irq, void *p)
{
	struct platform_device *arduino_pdev = p;
	struct arduino_irq_dev *arduino;

	arduino = platform_get_drvdata(arduino_pdev);
	tasklet_schedule(&arduino->t);

	dev_info(&arduino_pdev->dev, "irq=%d top-half executed", irq);
	return IRQ_HANDLED;
}

static void arduino_irq_bottom_half(unsigned long data)
{
	struct arduino_irq_dev *arduino;
	struct platform_device *arduino_pdev;
        arduino_pdev = (struct platform_device*)data;
	arduino = platform_get_drvdata(arduino_pdev);
        dev_info(&arduino_pdev->dev, 
		"irq=%d bottom-half executed", arduino->irq);
        return;
}

static int arduino_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct arduino_irq_dev *arduino;
	int ret = 0;

	arduino = devm_kzalloc(dev, sizeof(struct arduino_irq_dev), GFP_KERNEL);
	if (!arduino) {
		dev_err(dev, "failed to allocate memory");
		return -ENOMEM;
	}
	arduino->pdev = pdev;
	platform_set_drvdata(pdev, arduino);
    
	arduino->irq = platform_get_irq(pdev, 0);
	if (arduino->irq < 0) {
		dev_err(dev, "failed to get irq");
		return arduino->irq;
	}

	tasklet_init(&arduino->t, arduino_irq_bottom_half, 
			(unsigned long)(pdev));

	ret = devm_request_irq(dev, arduino->irq, arduino_irq_top_half,
			IRQF_TRIGGER_NONE, dev_name(&pdev->dev), pdev);
	if (ret < 0) {
		dev_err(dev, "failed to request IRQ");
		return ret;
    	}

	dev_info(dev, "successfully probed arduino!");
	return 0;
}

static int arduino_remove(struct platform_device *pdev)
{
	struct arduino_irq_dev *arduino;

	arduino = platform_get_drvdata(pdev);
	tasklet_disable(&arduino->t);
	return 0;
}

static const struct of_device_id arduino_ids[] = {
	{.compatible = "arduino,uno-irq",},
	{}
};

static struct platform_driver arduino_irq_driver = {
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = arduino_ids,
	},
	.probe = arduino_probe,
	.remove = arduino_remove
};
MODULE_LICENSE("GPL");
module_platform_driver(arduino_irq_driver);
